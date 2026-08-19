#include <QGuiApplication>
#include <QComboBox>
#include <QInputDialog>

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QDockWidget>
#include <QTreeView>
#include <QItemSelectionModel>

#include <QMouseEvent>

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QProgressDialog>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStringList>
#include <QStatusBar>
#include <QTabWidget>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

#include "MainWindow.h"
#include "Display.h"
#include "RecentFileList.h"
#include "engine/graph/RenderGraphExecutionTrace.h"
#include "engine/graph/RenderGraphRequest.h"
#include "engine/graph/RenderPassState.h"
#include "engine/raytracer/Raytracer.h"
#include "engine/raster/OpenGLRasterizer.h"
#include "engine/raster/RasterBackend.h"
#include "render/tonemap/TonemapFactory.h"
#include "render/viewplanes/ViewPlane.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"
#include "render/lights/PointLight.h"
#include "render/cameras/PinholeCamera.h"
#include "core/math/HitPointInterval.h"

#include "mcp/McpConfigWriter.h"
#include "mcp/McpServer.h"
#include "mcp/SceneEditingTools.h"
#include "mcp/SceneEditor.h"

#include "widgets/world/PropertyEditorWidget.h"
#include "widgets/world/PreviewDisplayWidget.h"
#include "widgets/world/RenderGraphInspectorWidget.h"
#include "widgets/world/RenderGraphTracePreviewWidget.h"
#include "widgets/world/SceneModel.h"
#include "widgets/world/RenderWindow.h"

#include "world/objects/Scene.h"
#include "world/objects/Camera.h"
#include "world/objects/SourceAsset.h"
#include "world/objects/Sphere.h"
#include "world/objects/Box.h"
#include "world/objects/Cylinder.h"
#include "world/objects/Ring.h"
#include "world/objects/Torus.h"
#include "world/objects/ScriptedSurface.h"
#include "world/objects/Group.h"
#include "world/objects/StepVisibilityEvaluator.h"

#include "world/objects/Intersection.h"
#include "world/objects/Union.h"
#include "world/objects/Difference.h"
#include "world/objects/MinkowskiSum.h"
#include "world/objects/ConvexHull.h"

#include "world/objects/MatteMaterial.h"
#include "world/objects/PhongMaterial.h"
#include "world/objects/TransparentMaterial.h"
#include "world/objects/ReflectiveMaterial.h"

#include "world/objects/ConstantColorTexture.h"
#include "world/objects/CheckerBoardTexture.h"

#include "world/objects/DirectionalLight.h"
#include "world/objects/PointLight.h"

#include "world/objects/PinholeCamera.h"
#include "world/objects/FishEyeCamera.h"
#include "world/objects/OrthographicCamera.h"
#include "world/objects/SphericalCamera.h"
#include "world/objects/ThinLensCamera.h"
#include "world/objects/TiltShiftCamera.h"
#include "world/objects/EquirectangularCamera.h"
#include "world/import/ImportOptions.h"
#include "world/import/ImportResult.h"
#include "world/import/SceneImporter.h"
#include "world/import/SceneImporterRegistry.h"
#include "core/util/QStringUtil.h"

namespace {
  using PropertyRows = QVector<QPair<QString, QString>>;

  struct PlaybackIndexRange {
    bool enabled{false};
    int first{0};
    int last{0};
    int count{0};
  };

  struct OpenedScene {
    std::unique_ptr<::Scene> scene;
    world::ImportResult importResult;
    bool nativeSceneFile{false};
    QString errorMessage;
  };

  struct ImportedElement {
    std::unique_ptr<Element> root;
    world::ImportResult importResult;
    QString errorMessage;
  };

  class SceneImporterThreadSupport {
  protected:
    world::ImportOptions defaultImportOptionsFor(world::SceneImporter& importer) const {
      world::ImportOptions options;
      for (const auto& option : importer.optionSchema()) {
        if (option.defaultValue.isValid())
          options.setValue(option.name, option.defaultValue);
      }
      return options;
    }

    void moveElementTreeToGuiThread(Element* element) const {
      if (element && qApp)
        element->moveToThread(qApp->thread());
    }

    std::unique_ptr<Element> wrapDirectImportRoot(const QString& fileName,
                                                  const world::SceneImporter& importer,
                                                  const world::ImportOptions& importOptions,
                                                  std::unique_ptr<Element> root) const {
      auto asset = std::make_unique<SourceAsset>();
      asset->setName(QFileInfo(fileName).completeBaseName());
      asset->setSourcePath(fileName);
      asset->setFormat(importer.name());
      asset->setImportOptions(importOptions.values());
      asset->adoptGeneratedRoot(std::move(root));
      return asset;
    }
  };

  class SceneOpenThread : public QThread, private SceneImporterThreadSupport {
  public:
    explicit SceneOpenThread(QString fileName, QObject* parent = nullptr)
        : QThread(parent),
          m_fileName(std::move(fileName)) {
    }

    OpenedScene takeOpenedScene() {
      return std::move(m_openedScene);
    }

  protected:
    void run() override {
      m_openedScene = loadScene();
    }

  private:
    OpenedScene loadScene() const {
      OpenedScene opened;
      const QString suffix = QFileInfo(m_fileName).suffix();
      std::unique_ptr<world::SceneImporter> importer;
      if (suffix.compare("json", Qt::CaseInsensitive) != 0) {
        importer = world::SceneImporterRegistry::self().createForFile(m_fileName);
      }

      if (!importer) {
        opened.nativeSceneFile = true;
        auto scene = std::make_unique<::Scene>(nullptr);
        try {
          if (!scene->load(m_fileName)) {
            opened.errorMessage = QString("Could not load %1").arg(m_fileName);
            return opened;
          }
        } catch (const std::exception& error) {
          opened.errorMessage = QString("Could not load %1: %2").arg(m_fileName, error.what());
          return opened;
        }
        moveElementTreeToGuiThread(scene.get());
        opened.scene = std::move(scene);
        return opened;
      }

      opened.nativeSceneFile = false;
      world::ImportOptions importOptions = defaultImportOptionsFor(*importer);
      opened.importResult = importer->importFile(m_fileName, importOptions);
      if (opened.importResult.failed()) {
        opened.errorMessage = QString("Could not import %1").arg(m_fileName);
        for (const auto& diagnostic : opened.importResult.diagnostics()) {
          if (diagnostic.isError()) {
            opened.errorMessage += QString(": %1").arg(diagnostic.message);
            break;
          }
        }
        return opened;
      }

      auto root = opened.importResult.takeRoot();
      if (auto* sceneRoot = qobject_cast<::Scene*>(root.get())) {
        root.release();
        moveElementTreeToGuiThread(sceneRoot);
        opened.scene = std::unique_ptr<::Scene>(sceneRoot);
        return opened;
      }

      auto scene = std::make_unique<::Scene>(nullptr);
      Element* importedRoot = nullptr;
      if (importer->wrapDirectImportInSourceAsset()) {
        root = wrapDirectImportRoot(m_fileName, *importer, importOptions, std::move(root));
        importedRoot = root.get();
        scene->addChild(std::move(root));
      } else {
        importedRoot = root.get();
        scene->addChild(std::move(root));
      }
      if (importedRoot)
        importer->configureImportedScene(*scene, *importedRoot, importOptions);
      scene->resolveElementReferences();
      moveElementTreeToGuiThread(scene.get());
      opened.scene = std::move(scene);
      return opened;
    }

    QString m_fileName;
    OpenedScene m_openedScene;
  };

  class SceneImportThread : public QThread, private SceneImporterThreadSupport {
  public:
    explicit SceneImportThread(QString fileName, QObject* parent = nullptr)
        : QThread(parent),
          m_fileName(std::move(fileName)) {
    }

    ImportedElement takeImportedElement() {
      return std::move(m_importedElement);
    }

  protected:
    void run() override {
      m_importedElement = loadElement();
    }

  private:
    ImportedElement loadElement() const {
      ImportedElement imported;
      auto importer = world::SceneImporterRegistry::self().createForFile(m_fileName);
      if (!importer) {
        imported.errorMessage = QString("No scene importer registered for %1").arg(m_fileName);
        return imported;
      }

      world::ImportOptions importOptions = defaultImportOptionsFor(*importer);
      imported.importResult = importer->importFile(m_fileName, importOptions);
      if (imported.importResult.failed()) {
        imported.errorMessage = QString("Could not import %1").arg(m_fileName);
        for (const auto& diagnostic : imported.importResult.diagnostics()) {
          if (diagnostic.isError()) {
            imported.errorMessage += QString(": %1").arg(diagnostic.message);
            break;
          }
        }
        return imported;
      }

      auto root = imported.importResult.takeRoot();
      if (!root) {
        imported.errorMessage =
          QString("Importer did not return a root object for %1").arg(m_fileName);
        return imported;
      }

      if (importer->wrapDirectImportInSourceAsset()) {
        root = wrapDirectImportRoot(m_fileName, *importer, importOptions, std::move(root));
      }

      importer->configureImportedRoot(*root, importOptions);
      moveElementTreeToGuiThread(root.get());
      imported.root = std::move(root);
      return imported;
    }

    QString m_fileName;
    ImportedElement m_importedElement;
  };

  class PreviewEngineIntentDefinition {
  public:
    virtual ~PreviewEngineIntentDefinition() = default;

    virtual bool matches(RenderDisplay::EngineKind kind) const = 0;
    virtual void apply(engine::graph::RenderGraphRequest& request,
                       engine::graph::RenderViewMode previewViewMode) const = 0;
  };

  class RaytracerPreviewIntentDefinition : public PreviewEngineIntentDefinition {
  public:
    bool matches(RenderDisplay::EngineKind kind) const override {
      return kind == RenderDisplay::EngineKind::Raytracer;
    }

    void apply(engine::graph::RenderGraphRequest& request,
               engine::graph::RenderViewMode previewViewMode) const override {
      request.setExecutorOverride(engine::graph::RenderExecutorPreference::Raytracer)
        .setViewModeOverride(previewViewMode);
    }
  };

  class RasterizerPreviewIntentDefinition : public PreviewEngineIntentDefinition {
  public:
    bool matches(RenderDisplay::EngineKind kind) const override {
      return kind == RenderDisplay::EngineKind::Rasterizer;
    }

    void apply(engine::graph::RenderGraphRequest& request,
               engine::graph::RenderViewMode previewViewMode) const override {
      request.setExecutorOverride(engine::graph::RenderExecutorPreference::Rasterizer)
        .setViewModeOverride(previewViewMode);
    }
  };

  class WavefrontPreviewIntentDefinition : public PreviewEngineIntentDefinition {
  public:
    bool matches(RenderDisplay::EngineKind kind) const override {
      return kind == RenderDisplay::EngineKind::Wavefront;
    }

    void apply(engine::graph::RenderGraphRequest& request,
               engine::graph::RenderViewMode previewViewMode) const override {
      request.setExecutorOverride(engine::graph::RenderExecutorPreference::Wavefront)
        .setViewModeOverride(previewViewMode);
    }
  };

  class PathTracerPreviewIntentDefinition : public PreviewEngineIntentDefinition {
  public:
    bool matches(RenderDisplay::EngineKind kind) const override {
      return kind == RenderDisplay::EngineKind::PathTracer;
    }

    void apply(engine::graph::RenderGraphRequest& request,
               engine::graph::RenderViewMode previewViewMode) const override {
      request.setExecutorOverride(engine::graph::RenderExecutorPreference::PathTracer)
        .setViewModeOverride(previewViewMode);
    }
  };

  class ScalarPathTracerPreviewIntentDefinition : public PreviewEngineIntentDefinition {
  public:
    bool matches(RenderDisplay::EngineKind kind) const override {
      return kind == RenderDisplay::EngineKind::ScalarPathTracer;
    }

    void apply(engine::graph::RenderGraphRequest& request,
               engine::graph::RenderViewMode previewViewMode) const override {
      engine::graph::RenderIntent intent = request.baseIntent();
      intent.engineOptions.raytracer().setIntegrator("pathtracer");
      request.setBaseIntent(std::move(intent))
        .setExecutorOverride(engine::graph::RenderExecutorPreference::Raytracer)
        .setViewModeOverride(previewViewMode);
    }
  };

  class WireframePreviewIntentDefinition : public PreviewEngineIntentDefinition {
  public:
    bool matches(RenderDisplay::EngineKind kind) const override {
      return kind == RenderDisplay::EngineKind::Wireframe;
    }

    void apply(engine::graph::RenderGraphRequest& request,
               engine::graph::RenderViewMode previewViewMode) const override {
      request.setExecutorOverride(engine::graph::RenderExecutorPreference::Wireframe)
        .setViewModeOverride(previewViewMode == engine::graph::RenderViewMode::Beauty
                               ? engine::graph::RenderViewMode::Wireframe
                               : previewViewMode);
    }
  };

  const std::vector<const PreviewEngineIntentDefinition*>& previewEngineIntentDefinitions() {
    static const RaytracerPreviewIntentDefinition raytracer;
    static const ScalarPathTracerPreviewIntentDefinition scalarPathTracer;
    static const WavefrontPreviewIntentDefinition wavefront;
    static const PathTracerPreviewIntentDefinition pathTracer;
    static const RasterizerPreviewIntentDefinition rasterizer;
    static const WireframePreviewIntentDefinition wireframe;
    static const std::vector<const PreviewEngineIntentDefinition*> result = {
      &raytracer, &scalarPathTracer, &pathTracer, &wavefront, &rasterizer, &wireframe};
    return result;
  }

  const PreviewEngineIntentDefinition&
  previewEngineIntentDefinition(RenderDisplay::EngineKind kind) {
    const auto& all = previewEngineIntentDefinitions();
    const auto it = std::find_if(all.begin(), all.end(),
                                 [&](const auto* definition) { return definition->matches(kind); });
    if (it == all.end()) {
      throw std::runtime_error("unsupported preview engine kind");
    }
    return **it;
  }

  QString producerText(const engine::graph::RenderPlan& plan,
                       const engine::graph::RenderResourceId& resource) {
    const auto* producer = plan.producerOf(resource);
    return producer ? qstr(producer->id) : QStringLiteral("-");
  }

  QString consumerText(const engine::graph::RenderPlan& plan,
                       const engine::graph::RenderResourceId& resource) {
    QStringList values;
    for (const auto* consumer : plan.consumersOf(resource))
      values << qstr(consumer->id);
    return dashIfEmpty(values.join(QStringLiteral(", ")));
  }

  QString resourceSizeText(const engine::graph::RenderResourceDescriptor& resource) {
    return QStringLiteral("%1x%2, %3 sample(s)")
      .arg(resource.width)
      .arg(resource.height)
      .arg(resource.sampleCount);
  }

  const engine::graph::RenderGraphResourceSnapshot*
  cacheSnapshotForResource(const engine::graph::RenderGraphExecutionTrace* trace,
                           const engine::graph::RenderResourceId& resource) {
    if (!trace)
      return nullptr;

    const auto outputs = trace->outputSnapshotsForResource(resource);
    if (!outputs.empty())
      return outputs.back();

    const auto inputs = trace->inputSnapshotsForResource(resource);
    return inputs.empty() ? nullptr : inputs.back();
  }

  void addRow(PropertyRows& rows, const QString& name, const QString& value) {
    rows.push_back({name, dashIfEmpty(value)});
  }

  void collectPlaybackIndices(const Element& root, std::set<int>& indices) {
    if (const auto* group = qobject_cast<const Group*>(&root)) {
      if (const auto stepIndex = group->stepIndex()) {
        indices.insert(*stepIndex);
      } else if (const auto layerIndex = group->layerIndex()) {
        indices.insert(*layerIndex);
      } else if (group->startTime() || group->endTime()) {
        const double start = group->startTime().value_or(group->endTime().value_or(0.0));
        const double end = group->endTime().value_or(start);
        if (std::isfinite(start) && std::isfinite(end)) {
          const double first = std::floor(std::min(start, end));
          const double last = std::ceil(std::max(start, end));
          if (first >= std::numeric_limits<int>::min() && last <= std::numeric_limits<int>::max()) {
            indices.insert(static_cast<int>(first));
            indices.insert(static_cast<int>(last));
          }
        }
      }
    }

    for (const auto* child : root.childElements())
      collectPlaybackIndices(*child, indices);
  }

  PlaybackIndexRange playbackIndexRange(const Scene* scene) {
    if (!scene)
      return {};

    std::set<int> indices;
    collectPlaybackIndices(*scene, indices);
    if (indices.empty())
      return {};

    return PlaybackIndexRange{true, *indices.begin(), *indices.rbegin(),
                              static_cast<int>(indices.size())};
  }

}

struct MainWindow::Private {
  inline Private()
      : timelineDockWidget(nullptr),
        timelineFrameSlider(nullptr),
        timelineFrameSpinBox(nullptr),
        timelineSummaryLabel(nullptr),
        playbackIndexSlider(nullptr),
        playbackIndexSpinBox(nullptr),
        playbackSummaryLabel(nullptr),
        renderGraphDockWidget(nullptr),
        renderGraphInspectorWidget(nullptr),
        renderWindow(nullptr),
        mcpServer(nullptr),
        sceneEditor(nullptr),
        itemSelectionModel(nullptr),
        scene(nullptr),
        currentFrame(0),
        currentPlaybackIndex(0),
        hasPlaybackIndex(false),
        currentElement(nullptr),
        fileMenu(nullptr),
        openRecentMenu(nullptr) {
  }

  QString fileName;
  RecentFileList recentFiles;

  RenderDisplay* display;
  QTabWidget* centralTabs;
  RenderGraphTracePreviewWidget* graphTracePreviewWidget;
  PreviewDisplayWidget* materialDisplay;
  PropertyEditorWidget* propertyEditorWidget;
  SceneModel* elementModel;
  QDockWidget* timelineDockWidget;
  QSlider* timelineFrameSlider;
  QSpinBox* timelineFrameSpinBox;
  QLabel* timelineSummaryLabel;
  QSlider* playbackIndexSlider;
  QSpinBox* playbackIndexSpinBox;
  QLabel* playbackSummaryLabel;
  QDockWidget* renderGraphDockWidget;
  RenderGraphInspectorWidget* renderGraphInspectorWidget;

  RenderWindow* renderWindow;
  mcp::McpServer* mcpServer;
  mcp::SceneEditor* sceneEditor;
  QItemSelectionModel* itemSelectionModel;

  Scene* scene;
  int currentFrame;
  int currentPlaybackIndex;
  bool hasPlaybackIndex;

  Element* currentElement;
  QModelIndex currentIndex;

  QMenu* fileMenu;
  QMenu* openRecentMenu;
  QMenu* editMenu;
  QMenu* renderMenu;
  QMenu* helpMenu;

  QAction* newAct;
  QAction* openAct;
  std::vector<QAction*> recentFileActs;
  QAction* importAct;
  QAction* saveAct;
  QAction* saveAsAct;

  QAction* addBoxAct;
  QAction* addSphereAct;
  QAction* addCylinderAct;
  QAction* addRingAct;
  QAction* addTorusAct;
  QAction* addScriptAct;
  QAction* addGroupAct;

  QAction* addIntersectionAct;
  QAction* addUnionAct;
  QAction* addDifferenceAct;
  QAction* addMinkowskiSumAct;
  QAction* addConvexHullAct;

  QAction* addMatteMaterialAct;
  QAction* addPhongMaterialAct;
  QAction* addTransparentMaterialAct;
  QAction* addReflectiveMaterialAct;

  QAction* addConstantColorTextureAct;
  QAction* addCheckerBoardTextureAct;

  QAction* addDirectionalLightAct;
  QAction* addPointLightAct;

  QAction* addPinholeCameraAct;
  QAction* addFishEyeCameraAct;
  QAction* addOrthographicCameraAct;
  QAction* addSphericalCameraAct;
  QAction* addThinLensCameraAct;
  QAction* addTiltShiftCameraAct;
  QAction* addEquirectangularCameraAct;

  QAction* deleteElementAct;

  QAction* moveForwardsAlongXAct;
  QAction* moveBackwardsAlongXAct;
  QAction* moveForwardsAlongYAct;
  QAction* moveBackwardsAlongYAct;
  QAction* moveForwardsAlongZAct;
  QAction* moveBackwardsAlongZAct;

  QAction* renderAct;
  QAction* previewUseSceneIntentAct;
  QAction* previewRaytracerAct;
  QAction* previewScalarPathTracerAct;
  QAction* previewPathTracerAct;
  QAction* previewWavefrontAct;
  QAction* previewWireframeAct;
  QAction* previewRasterizerAct;
  QAction* previewRasterizerShadowsAct;
  QAction* previewFpsOverlayAct;
  QAction* previewGraphTraceCaptureAct;
  QAction* previewRasterBackendCPUAct;
  QAction* previewRasterBackendOpenGLAct;
  QAction* previewPostAANoneAct;
  QAction* previewPostAAFxaaAct;
  QAction* previewPostAASmaaAct;
  QAction* previewViewBeautyAct;
  QAction* previewViewDepthAct;
  QAction* previewViewStencilAct;
  QAction* previewViewStencilCompositeAct;
  QAction* previewViewNormalAct;
  QAction* previewViewObjectIdAct;
  QAction* previewViewMaterialIdAct;
  QAction* previewViewWorldPositionAct;
  QAction* previewViewRasterCoverageCountAct;
  QAction* previewViewRasterDepthTestCountAct;
  QAction* previewViewRasterDepthPassCountAct;
  QAction* previewViewRasterShadeCountAct;
  QAction* previewViewRasterColorWriteCountAct;
  QAction* previewWireframeOverlayAct;
  QAction* previewTonemapLinearAct;
  QAction* previewTonemapReinhardAct;
  QAction* previewTonemapAcesAct;

  QAction* aspectStretchAct;
  QAction* aspectFitWidthAct;
  QAction* aspectFitHeightAct;
  QAction* aspectFitExactAct;
  QMenu* aspectRatioMenu;
  QAction* aspect16x9Act;
  QAction* aspect4x3Act;
  QAction* aspect1x1Act;
  QAction* aspect239x1Act;
  QAction* aspect21x9Act;

  QAction* aboutAct;
  QAction* helpAct;
};

MainWindow::~MainWindow() = default;

MainWindow::MainWindow()
    : QMainWindow(),
      p(std::make_unique<Private>()) {
  p->scene = new ::Scene(nullptr);

  p->display = new RenderDisplay(this);
  p->graphTracePreviewWidget = new RenderGraphTracePreviewWidget(this);
  p->centralTabs = new QTabWidget(this);
  p->centralTabs->setObjectName("modelerCentralPreviewTabs");
  p->centralTabs->addTab(p->display, tr("Preview"));
  p->centralTabs->addTab(p->graphTracePreviewWidget, tr("Graph Trace"));
  setCentralWidget(p->centralTabs);

  addDockWidget(Qt::LeftDockWidgetArea, createElementSelector());
  addDockWidget(Qt::RightDockWidgetArea, createPropertyEditor());
  addDockWidget(Qt::RightDockWidgetArea, createPreviewDisplay());
  addDockWidget(Qt::BottomDockWidgetArea, createTimelineControls());
  addDockWidget(Qt::BottomDockWidgetArea, createRenderGraphInspector());

  connect(this, SIGNAL(selectionChanged(Element*)), this, SLOT(updatePreviewWidget()));
  connect(this, SIGNAL(currentElementChanged()), this, SLOT(updatePreviewWidget()));
  connect(p->display, SIGNAL(renderGraphInputsChanged()), this, SLOT(updateRenderGraphInspector()));
  connect(p->display, &RenderDisplay::renderGraphExecutionStarted, p->renderGraphInspectorWidget,
          &RenderGraphInspectorWidget::clearExecutionState);
  connect(p->display, &RenderDisplay::renderGraphExecutionStarted, this, [this] {
    if (p->centralTabs && p->display)
      p->centralTabs->setCurrentWidget(p->display);
  });
  connect(p->display, &RenderDisplay::renderGraphPassStarted, p->renderGraphInspectorWidget,
          &RenderGraphInspectorWidget::passExecutionStarted);
  connect(p->display, &RenderDisplay::renderGraphPassFinished, p->renderGraphInspectorWidget,
          &RenderGraphInspectorWidget::passExecutionFinished);
  connect(p->display, &RenderDisplay::renderGraphPassFailed, p->renderGraphInspectorWidget,
          &RenderGraphInspectorWidget::passExecutionFailed);
  connect(p->display, &RenderDisplay::renderGraphActivePassesChanged, p->renderGraphInspectorWidget,
          &RenderGraphInspectorWidget::setActiveExecutionPasses);
  connect(p->display, &RenderWidget::finished, this, [this] {
    if (p->renderGraphInspectorWidget && p->display) {
      p->renderGraphInspectorWidget->setExecutionTrace(p->display->lastRenderGraphExecutionTrace());
    }
  });
  connect(p->display, &RenderWidget::renderFailed, this, [this](const QString& message) {
    statusBar()->showMessage(tr("Preview render failed: %1").arg(message));
  });
  connect(p->renderGraphInspectorWidget, SIGNAL(overridesChanged()), this,
          SLOT(renderGraphOverridesChanged()));
  connect(p->renderGraphInspectorWidget, SIGNAL(passSelected(QString)), this,
          SLOT(renderGraphPassSelected(QString)));
  connect(p->renderGraphInspectorWidget, SIGNAL(resourceSelected(QString)), this,
          SLOT(renderGraphResourceSelected(QString)));
  connect(p->renderGraphInspectorWidget, SIGNAL(selectedPassTraceChanged(QString)), this,
          SLOT(renderGraphPassTraceChanged(QString)));
  connect(p->renderGraphInspectorWidget, SIGNAL(selectedResourceTraceChanged(QString)), this,
          SLOT(renderGraphResourceTraceChanged(QString)));
  connect(p->renderGraphInspectorWidget, SIGNAL(graphExportRequested(QString, QByteArray)), this,
          SLOT(exportRenderGraph(QString, QByteArray)));

  createActions();
  loadRecentFiles();
  createMenus();

  p->renderWindow = new RenderWindow(nullptr);
  resetTimelineFrame();
  resetPlaybackIndex();
  updateRenderGraphInspector();
  p->display->setScene(p->scene);

  // Loopback-only MCP server (roadmap §4.6.i): starts as soon as the
  // Modeler has a scene to serve — including the blank scene a fresh
  // window opens with — and stops on window close (see closeEvent()).
  p->mcpServer = new mcp::McpServer([this]() { return p->scene; }, this);

  // Mutating tools (roadmap §4.6.i, v1 tool surface) go through the exact
  // same SceneModel/QItemSelectionModel the Elements dock uses, so
  // agent-driven edits fire the same rowsInserted/rowsRemoved/moveRows and
  // currentChanged signals a menu action would. elementChanged() re-runs
  // this window's usual post-edit reaction (mark changed, redraw, sync
  // playback, emit currentElementChanged()) exactly as PropertyEditorWidget's
  // changed(Element*) already does.
  p->sceneEditor =
    new mcp::SceneEditor([this]() { return p->scene; }, p->elementModel, p->itemSelectionModel, this);
  connect(p->sceneEditor, &mcp::SceneEditor::elementChanged, this, &MainWindow::elementChanged);
  mcp::registerSceneEditingTools(*p->mcpServer, *p->sceneEditor);

  if (p->mcpServer->start()) {
    const QString configPath = mcp::writeMcpConfig(*p->mcpServer);
    if (!configPath.isEmpty()) {
      statusBar()->showMessage(
        tr("MCP server listening on 127.0.0.1:%1 — config written to %2")
          .arg(p->mcpServer->port())
          .arg(configPath),
        8000);
    }
  }
}

void MainWindow::createActions() {
  p->newAct = new QAction(tr("&New"), this);
  p->newAct->setShortcuts(QKeySequence::New);
  p->newAct->setStatusTip(tr("Create a new file"));
  connect(p->newAct, SIGNAL(triggered()), this, SLOT(newFile()));

  p->openAct = new QAction(tr("&Open"), this);
  p->openAct->setShortcuts(QKeySequence::Open);
  p->openAct->setStatusTip(tr("Open a file from disk"));
  connect(p->openAct, SIGNAL(triggered()), this, SLOT(openFile()));

  p->recentFileActs.reserve(RecentFileList::limit);
  for (int i = 0; i != RecentFileList::limit; ++i) {
    auto* action = new QAction(this);
    action->setObjectName(QStringLiteral("recentFileAction%1").arg(i));
    action->setVisible(false);
    connect(action, SIGNAL(triggered()), this, SLOT(openRecentFile()));
    p->recentFileActs.push_back(action);
  }

  p->importAct = new QAction(tr("&Import"), this);
  p->importAct->setStatusTip(tr("Import a model into the current scene"));
  connect(p->importAct, SIGNAL(triggered()), this, SLOT(importFile()));

  p->saveAct = new QAction(tr("&Save"), this);
  p->saveAct->setShortcuts(QKeySequence::Save);
  p->saveAct->setStatusTip(tr("Save the current project to a file"));
  connect(p->saveAct, SIGNAL(triggered()), this, SLOT(saveFile()));

  p->saveAsAct = new QAction(tr("Save &As"), this);
  p->saveAsAct->setShortcuts(QKeySequence::SaveAs);
  p->saveAsAct->setStatusTip(tr("Save the current project to a new file"));
  connect(p->saveAsAct, SIGNAL(triggered()), this, SLOT(saveFileAs()));

  p->addBoxAct = new QAction(tr("Box"), this);
  p->addBoxAct->setStatusTip(tr("Add a Box to the scene"));
  connect(p->addBoxAct, SIGNAL(triggered()), this, SLOT(addBox()));

  p->addSphereAct = new QAction(tr("Sphere"), this);
  p->addSphereAct->setStatusTip(tr("Add a Sphere to the scene"));
  connect(p->addSphereAct, SIGNAL(triggered()), this, SLOT(addSphere()));

  p->addCylinderAct = new QAction(tr("Cylinder"), this);
  p->addCylinderAct->setStatusTip(tr("Add a Cylinder to the scene"));
  connect(p->addCylinderAct, SIGNAL(triggered()), this, SLOT(addCylinder()));

  p->addRingAct = new QAction(tr("Ring"), this);
  p->addRingAct->setStatusTip(tr("Add a Ring to the scene"));
  connect(p->addRingAct, SIGNAL(triggered()), this, SLOT(addRing()));

  p->addTorusAct = new QAction(tr("Torus"), this);
  p->addTorusAct->setStatusTip(tr("Add a Torus to the scene"));
  connect(p->addTorusAct, SIGNAL(triggered()), this, SLOT(addTorus()));

  p->addScriptAct = new QAction(tr("Script"), this);
  p->addScriptAct->setStatusTip(tr("Add a Script to the scene"));
  connect(p->addScriptAct, SIGNAL(triggered()), this, SLOT(addScript()));

  p->addGroupAct = new QAction(tr("Group"), this);
  p->addGroupAct->setStatusTip(tr("Add a Group to organize scene elements"));
  connect(p->addGroupAct, SIGNAL(triggered()), this, SLOT(addGroup()));

  p->addIntersectionAct = new QAction(tr("Intersection"), this);
  p->addIntersectionAct->setStatusTip(tr("Add an intersection to the scene"));
  connect(p->addIntersectionAct, SIGNAL(triggered()), this, SLOT(addIntersection()));

  p->addUnionAct = new QAction(tr("Union"), this);
  p->addUnionAct->setStatusTip(tr("Add a union to the scene"));
  connect(p->addUnionAct, SIGNAL(triggered()), this, SLOT(addUnion()));

  p->addDifferenceAct = new QAction(tr("Difference"), this);
  p->addDifferenceAct->setStatusTip(tr("Add a difference to the scene"));
  connect(p->addDifferenceAct, SIGNAL(triggered()), this, SLOT(addDifference()));

  p->addMinkowskiSumAct = new QAction(tr("Minkowski Sum"), this);
  p->addMinkowskiSumAct->setStatusTip(tr("Add a Minkowski sum to the scene"));
  connect(p->addMinkowskiSumAct, SIGNAL(triggered()), this, SLOT(addMinkowskiSum()));

  p->addConvexHullAct = new QAction(tr("Convex Hull"), this);
  p->addConvexHullAct->setStatusTip(tr("Add a Convex Hull to the scene"));
  connect(p->addConvexHullAct, SIGNAL(triggered()), this, SLOT(addConvexHull()));

  p->addMatteMaterialAct = new QAction(tr("Matte Material"), this);
  p->addMatteMaterialAct->setStatusTip(tr("Add a matte material to the scene"));
  connect(p->addMatteMaterialAct, SIGNAL(triggered()), this, SLOT(addMatteMaterial()));

  p->addPhongMaterialAct = new QAction(tr("Phong Material"), this);
  p->addPhongMaterialAct->setStatusTip(tr("Add a Phong material to the scene"));
  connect(p->addPhongMaterialAct, SIGNAL(triggered()), this, SLOT(addPhongMaterial()));

  p->addTransparentMaterialAct = new QAction(tr("Transparent Material"), this);
  p->addTransparentMaterialAct->setStatusTip(tr("Add a transparent material to the scene"));
  connect(p->addTransparentMaterialAct, SIGNAL(triggered()), this, SLOT(addTransparentMaterial()));

  p->addReflectiveMaterialAct = new QAction(tr("Reflective Material"), this);
  p->addReflectiveMaterialAct->setStatusTip(tr("Add a reflective material to the scene"));
  connect(p->addReflectiveMaterialAct, SIGNAL(triggered()), this, SLOT(addReflectiveMaterial()));

  p->addConstantColorTextureAct = new QAction(tr("Constant Color"), this);
  p->addConstantColorTextureAct->setStatusTip(tr("Add a constant color texture to the scene"));
  connect(p->addConstantColorTextureAct, SIGNAL(triggered()), this,
          SLOT(addConstantColorTexture()));

  p->addCheckerBoardTextureAct = new QAction(tr("Checker Board"), this);
  p->addCheckerBoardTextureAct->setStatusTip(tr("Add a checker board texture to the scene"));
  connect(p->addCheckerBoardTextureAct, SIGNAL(triggered()), this, SLOT(addCheckerBoardTexture()));

  p->addDirectionalLightAct = new QAction(tr("Directional Light"), this);
  p->addDirectionalLightAct->setStatusTip(tr("Add a directional light to the scene"));
  connect(p->addDirectionalLightAct, SIGNAL(triggered()), this, SLOT(addDirectionalLight()));

  p->addPointLightAct = new QAction(tr("Point Light"), this);
  p->addPointLightAct->setStatusTip(tr("Add a point light to the scene"));
  connect(p->addPointLightAct, SIGNAL(triggered()), this, SLOT(addPointLight()));

  p->addPinholeCameraAct = new QAction(tr("Pinhole Camera"), this);
  p->addPinholeCameraAct->setStatusTip(tr("Add a pinhole camera to the scene"));
  connect(p->addPinholeCameraAct, SIGNAL(triggered()), this, SLOT(addPinholeCamera()));

  p->addFishEyeCameraAct = new QAction(tr("Fish Eye Camera"), this);
  p->addFishEyeCameraAct->setStatusTip(tr("Add a fish eye camera to the scene"));
  connect(p->addFishEyeCameraAct, SIGNAL(triggered()), this, SLOT(addFishEyeCamera()));

  p->addOrthographicCameraAct = new QAction(tr("Orthographic Camera"), this);
  p->addOrthographicCameraAct->setStatusTip(tr("Add an orthographic camera to the scene"));
  connect(p->addOrthographicCameraAct, SIGNAL(triggered()), this, SLOT(addOrthographicCamera()));

  p->addSphericalCameraAct = new QAction(tr("Spherical Camera"), this);
  p->addSphericalCameraAct->setStatusTip(tr("Add a spherical camera to the scene"));
  connect(p->addSphericalCameraAct, SIGNAL(triggered()), this, SLOT(addSphericalCamera()));

  p->addThinLensCameraAct = new QAction(tr("Thin Lens Camera (DOF)"), this);
  p->addThinLensCameraAct->setStatusTip(
    tr("Add a thin-lens camera with depth-of-field to the scene"));
  connect(p->addThinLensCameraAct, SIGNAL(triggered()), this, SLOT(addThinLensCamera()));

  p->addTiltShiftCameraAct = new QAction(tr("Tilt-Shift Camera"), this);
  p->addTiltShiftCameraAct->setStatusTip(
    tr("Add a tilt-shift / Scheimpflug camera (DOF + tilted focal plane) to the scene"));
  connect(p->addTiltShiftCameraAct, SIGNAL(triggered()), this, SLOT(addTiltShiftCamera()));

  p->addEquirectangularCameraAct = new QAction(tr("Equirectangular Camera (360°)"), this);
  p->addEquirectangularCameraAct->setStatusTip(
    tr("Add a full-sphere panorama camera to the scene (render at 2:1 aspect)"));
  connect(p->addEquirectangularCameraAct, SIGNAL(triggered()), this,
          SLOT(addEquirectangularCamera()));

  p->moveForwardsAlongXAct = new QAction(tr("Move forwards along X axis"), this);
  p->moveForwardsAlongXAct->setShortcuts(QList<QKeySequence>{
    QKeySequence(Qt::META | Qt::Key_Up), QKeySequence(Qt::SHIFT | Qt::META | Qt::Key_Up)});
  p->moveForwardsAlongXAct->setStatusTip(tr("Moves the current element forwards along the X axis"));
  connect(p->moveForwardsAlongXAct, SIGNAL(triggered()), this, SLOT(moveForwardsAlongX()));

  p->moveBackwardsAlongXAct = new QAction(tr("Move backwards along X axis"), this);
  p->moveBackwardsAlongXAct->setShortcuts(QList<QKeySequence>{
    QKeySequence(Qt::META | Qt::Key_Down), QKeySequence(Qt::SHIFT | Qt::META | Qt::Key_Down)});
  p->moveBackwardsAlongXAct->setStatusTip(
    tr("Moves the current element backwards along the X axis"));
  connect(p->moveBackwardsAlongXAct, SIGNAL(triggered()), this, SLOT(moveBackwardsAlongX()));

  p->moveForwardsAlongYAct = new QAction(tr("Move forwards along Y axis"), this);
  p->moveForwardsAlongYAct->setShortcuts(QList<QKeySequence>{
    QKeySequence(Qt::ALT | Qt::Key_Up), QKeySequence(Qt::SHIFT | Qt::ALT | Qt::Key_Up)});
  p->moveForwardsAlongYAct->setStatusTip(tr("Moves the current element forwards along the Y axis"));
  connect(p->moveForwardsAlongYAct, SIGNAL(triggered()), this, SLOT(moveForwardsAlongY()));

  p->moveBackwardsAlongYAct = new QAction(tr("Move backwards along Y axis"), this);
  p->moveBackwardsAlongYAct->setShortcuts(QList<QKeySequence>{
    QKeySequence(Qt::ALT | Qt::Key_Down), QKeySequence(Qt::SHIFT | Qt::ALT | Qt::Key_Down)});
  p->moveBackwardsAlongYAct->setStatusTip(
    tr("Moves the current element backwards along the Y axis"));
  connect(p->moveBackwardsAlongYAct, SIGNAL(triggered()), this, SLOT(moveBackwardsAlongY()));

  p->moveForwardsAlongZAct = new QAction(tr("Move forwards along Z axis"), this);
  p->moveForwardsAlongZAct->setShortcuts(QList<QKeySequence>{
    QKeySequence(Qt::CTRL | Qt::Key_Up), QKeySequence(Qt::SHIFT | Qt::CTRL | Qt::Key_Up)});
  p->moveForwardsAlongZAct->setStatusTip(tr("Moves the current element forwards along the Z axis"));
  connect(p->moveForwardsAlongZAct, SIGNAL(triggered()), this, SLOT(moveForwardsAlongZ()));

  p->moveBackwardsAlongZAct = new QAction(tr("Move backwards along Z axis"), this);
  p->moveBackwardsAlongZAct->setShortcuts(QList<QKeySequence>{
    QKeySequence(Qt::CTRL | Qt::Key_Down), QKeySequence(Qt::SHIFT | Qt::CTRL | Qt::Key_Down)});
  p->moveBackwardsAlongZAct->setStatusTip(
    tr("Moves the current element backwards along the Z axis"));
  connect(p->moveBackwardsAlongZAct, SIGNAL(triggered()), this, SLOT(moveBackwardsAlongZ()));

  p->aboutAct = new QAction(tr("&About"), this);
  p->aboutAct->setStatusTip(tr("Show the application's About box"));
  connect(p->aboutAct, SIGNAL(triggered()), this, SLOT(about()));

  p->deleteElementAct = new QAction(tr("&Delete"), this);
  p->deleteElementAct->setShortcuts(QKeySequence::Delete);
  p->deleteElementAct->setStatusTip(tr("Delete selected element"));
  p->deleteElementAct->setEnabled(false);
  connect(p->deleteElementAct, SIGNAL(triggered()), this, SLOT(deleteElement()));

  p->renderAct = new QAction(tr("&Render"), this);
  p->renderAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
  p->renderAct->setStatusTip(tr("Render current scene"));
  connect(p->renderAct, SIGNAL(triggered()), this, SLOT(render()));

  p->previewUseSceneIntentAct = new QAction(tr("Use Scene Render &Settings"), this);
  p->previewUseSceneIntentAct->setStatusTip(
    tr("Compile the live preview directly from the scene's saved render intent"));
  p->previewUseSceneIntentAct->setCheckable(true);
  p->previewUseSceneIntentAct->setChecked(true);
  connect(p->previewUseSceneIntentAct, SIGNAL(triggered(bool)), this,
          SLOT(useSceneRenderIntentPreview(bool)));

  // Preview-engine selection — radio-style via a QActionGroup so
  // exactly one is active at a time. Defaults to Raytracer to match
  // the historical behaviour.
  p->previewRaytracerAct = new QAction(tr("&Raytracer"), this);
  p->previewRaytracerAct->setStatusTip(tr("Show the modeling preview as a ray-traced render"));
  p->previewRaytracerAct->setCheckable(true);
  p->previewRaytracerAct->setChecked(true);
  connect(p->previewRaytracerAct, SIGNAL(triggered()), this, SLOT(usePreviewRaytracer()));

  p->previewWavefrontAct = new QAction(tr("Wave&front"), this);
  p->previewWavefrontAct->setStatusTip(
    tr("Show the modeling preview through the depth-major wavefront ray executor"));
  p->previewWavefrontAct->setCheckable(true);
  connect(p->previewWavefrontAct, SIGNAL(triggered()), this, SLOT(usePreviewWavefront()));

  p->previewPathTracerAct = new QAction(tr("Path &Tracer"), this);
  p->previewPathTracerAct->setStatusTip(
    tr("Show the modeling preview as a wavefront-scheduled path-traced render"));
  p->previewPathTracerAct->setCheckable(true);
  connect(p->previewPathTracerAct, SIGNAL(triggered()), this, SLOT(usePreviewPathTracer()));

  p->previewScalarPathTracerAct = new QAction(tr("Scalar Path Tracer"), this);
  p->previewScalarPathTracerAct->setStatusTip(
    tr("Show the modeling preview as scalar path tracing through the raytracer executor"));
  p->previewScalarPathTracerAct->setCheckable(true);
  connect(p->previewScalarPathTracerAct, SIGNAL(triggered()), this,
          SLOT(usePreviewScalarPathTracer()));

  p->previewWireframeAct = new QAction(tr("&Wireframe"), this);
  p->previewWireframeAct->setStatusTip(
    tr("Show the modeling preview as a wireframe (faster, geometry-only)"));
  p->previewWireframeAct->setCheckable(true);
  connect(p->previewWireframeAct, SIGNAL(triggered()), this, SLOT(usePreviewWireframe()));

  p->previewRasterizerAct = new QAction(tr("Ras&terizer"), this);
  p->previewRasterizerAct->setStatusTip(
    tr("Show the modeling preview as a software-rasterized render (filled triangles, Lambertian "
       "shading, no recursion)"));
  p->previewRasterizerAct->setCheckable(true);
  connect(p->previewRasterizerAct, SIGNAL(triggered()), this, SLOT(usePreviewRasterizer()));

  p->previewRasterizerShadowsAct = new QAction(tr("Rasterizer Preview &Shadows"), this);
  p->previewRasterizerShadowsAct->setStatusTip(
    tr("Enable directional shadow maps in the live rasterizer preview"));
  p->previewRasterizerShadowsAct->setCheckable(true);
  connect(p->previewRasterizerShadowsAct, SIGNAL(triggered(bool)), this,
          SLOT(setPreviewRasterizerShadows(bool)));

  p->previewFpsOverlayAct = new QAction(tr("&FPS Overlay"), this);
  p->previewFpsOverlayAct->setStatusTip(
    tr("Show a small mean frame time / FPS readout in the top-right of the preview window"));
  p->previewFpsOverlayAct->setCheckable(true);
  p->previewFpsOverlayAct->setChecked(false);
  connect(p->previewFpsOverlayAct, SIGNAL(triggered(bool)), this, SLOT(setPreviewFpsOverlay(bool)));

  p->previewGraphTraceCaptureAct = new QAction(tr("Capture Graph &Trace"), this);
  p->previewGraphTraceCaptureAct->setObjectName(QStringLiteral("previewGraphTraceCaptureAct"));
  p->previewGraphTraceCaptureAct->setStatusTip(
    tr("Capture per-pass graph trace images and metadata for the live preview"));
  p->previewGraphTraceCaptureAct->setCheckable(true);
  p->previewGraphTraceCaptureAct->setChecked(false);
  connect(p->previewGraphTraceCaptureAct, SIGNAL(triggered(bool)), this,
          SLOT(setPreviewGraphTraceCapture(bool)));

  p->previewRasterBackendCPUAct = new QAction(tr("&CPU"), this);
  p->previewRasterBackendCPUAct->setStatusTip(
    tr("Use the software CPU rasterizer for live raster preview passes"));
  p->previewRasterBackendCPUAct->setCheckable(true);
  p->previewRasterBackendCPUAct->setChecked(true);
  connect(p->previewRasterBackendCPUAct, SIGNAL(triggered()), this,
          SLOT(setPreviewRasterBackendCPU()));

  p->previewRasterBackendOpenGLAct = new QAction(tr("&OpenGL"), this);
  p->previewRasterBackendOpenGLAct->setStatusTip(
    tr("Use the experimental OpenGL raster backend for live raster preview passes"));
  p->previewRasterBackendOpenGLAct->setToolTip(
    QString::fromStdString(engine::raster::OpenGLRasterizer::statusMessage()));
  p->previewRasterBackendOpenGLAct->setCheckable(true);
  connect(p->previewRasterBackendOpenGLAct, SIGNAL(triggered()), this,
          SLOT(setPreviewRasterBackendOpenGL()));

  auto previewRasterBackendGroup = new QActionGroup(this);
  previewRasterBackendGroup->addAction(p->previewRasterBackendCPUAct);
  previewRasterBackendGroup->addAction(p->previewRasterBackendOpenGLAct);

  p->previewPostAANoneAct = new QAction(tr("&None"), this);
  p->previewPostAANoneAct->setStatusTip(
    tr("Disable image-space anti-aliasing in the live preview"));
  p->previewPostAANoneAct->setCheckable(true);
  p->previewPostAANoneAct->setChecked(true);
  connect(p->previewPostAANoneAct, SIGNAL(triggered()), this, SLOT(setPreviewPostAANone()));

  p->previewPostAAFxaaAct = new QAction(tr("&FXAA"), this);
  p->previewPostAAFxaaAct->setStatusTip(tr("Add a graph-visible FXAA pass to the live preview"));
  p->previewPostAAFxaaAct->setCheckable(true);
  connect(p->previewPostAAFxaaAct, SIGNAL(triggered()), this, SLOT(setPreviewPostAAFxaa()));

  p->previewPostAASmaaAct = new QAction(tr("&SMAA"), this);
  p->previewPostAASmaaAct->setStatusTip(tr("Add a graph-visible SMAA pass to the live preview"));
  p->previewPostAASmaaAct->setCheckable(true);
  connect(p->previewPostAASmaaAct, SIGNAL(triggered()), this, SLOT(setPreviewPostAASmaa()));

  auto previewPostAAGroup = new QActionGroup(this);
  previewPostAAGroup->addAction(p->previewPostAANoneAct);
  previewPostAAGroup->addAction(p->previewPostAAFxaaAct);
  previewPostAAGroup->addAction(p->previewPostAASmaaAct);

  p->previewViewBeautyAct = new QAction(tr("&Beauty"), this);
  p->previewViewBeautyAct->setStatusTip(tr("Show the live preview beauty view"));
  p->previewViewBeautyAct->setCheckable(true);
  p->previewViewBeautyAct->setChecked(true);
  connect(p->previewViewBeautyAct, SIGNAL(triggered()), this, SLOT(setPreviewViewBeauty()));

  p->previewViewDepthAct = new QAction(tr("&Depth"), this);
  p->previewViewDepthAct->setStatusTip(tr("Show the live preview depth AOV"));
  p->previewViewDepthAct->setCheckable(true);
  connect(p->previewViewDepthAct, SIGNAL(triggered()), this, SLOT(setPreviewViewDepth()));

  p->previewViewStencilAct = new QAction(tr("&Stencil"), this);
  p->previewViewStencilAct->setStatusTip(tr("Show the live preview stencil AOV"));
  p->previewViewStencilAct->setCheckable(true);
  connect(p->previewViewStencilAct, SIGNAL(triggered()), this, SLOT(setPreviewViewStencil()));

  p->previewViewStencilCompositeAct = new QAction(tr("Stencil &Composite"), this);
  p->previewViewStencilCompositeAct->setStatusTip(
    tr("Show the live preview stencil-composited raster and wireframe view"));
  p->previewViewStencilCompositeAct->setCheckable(true);
  connect(p->previewViewStencilCompositeAct, SIGNAL(triggered()), this,
          SLOT(setPreviewViewStencilComposite()));

  p->previewViewNormalAct = new QAction(tr("&Normal"), this);
  p->previewViewNormalAct->setStatusTip(tr("Show the live preview normal AOV"));
  p->previewViewNormalAct->setCheckable(true);
  connect(p->previewViewNormalAct, SIGNAL(triggered()), this, SLOT(setPreviewViewNormal()));

  p->previewViewObjectIdAct = new QAction(tr("&Object ID"), this);
  p->previewViewObjectIdAct->setStatusTip(tr("Show the live preview object-id AOV"));
  p->previewViewObjectIdAct->setCheckable(true);
  connect(p->previewViewObjectIdAct, SIGNAL(triggered()), this, SLOT(setPreviewViewObjectId()));

  p->previewViewMaterialIdAct = new QAction(tr("&Material ID"), this);
  p->previewViewMaterialIdAct->setStatusTip(tr("Show the live preview material-id AOV"));
  p->previewViewMaterialIdAct->setCheckable(true);
  connect(p->previewViewMaterialIdAct, SIGNAL(triggered()), this, SLOT(setPreviewViewMaterialId()));

  p->previewViewWorldPositionAct = new QAction(tr("&World Position"), this);
  p->previewViewWorldPositionAct->setStatusTip(tr("Show the live preview world-position AOV"));
  p->previewViewWorldPositionAct->setCheckable(true);
  connect(p->previewViewWorldPositionAct, SIGNAL(triggered()), this,
          SLOT(setPreviewViewWorldPosition()));

  p->previewViewRasterCoverageCountAct = new QAction(tr("Raster &Coverage Count"), this);
  p->previewViewRasterCoverageCountAct->setStatusTip(
    tr("Show covered raster samples per preview pixel"));
  p->previewViewRasterCoverageCountAct->setCheckable(true);
  connect(p->previewViewRasterCoverageCountAct, SIGNAL(triggered()), this,
          SLOT(setPreviewViewRasterCoverageCount()));

  p->previewViewRasterDepthTestCountAct = new QAction(tr("Raster Depth-&Test Count"), this);
  p->previewViewRasterDepthTestCountAct->setStatusTip(
    tr("Show depth-tested raster samples per preview pixel"));
  p->previewViewRasterDepthTestCountAct->setCheckable(true);
  connect(p->previewViewRasterDepthTestCountAct, SIGNAL(triggered()), this,
          SLOT(setPreviewViewRasterDepthTestCount()));

  p->previewViewRasterDepthPassCountAct = new QAction(tr("Raster Depth-&Pass Count"), this);
  p->previewViewRasterDepthPassCountAct->setStatusTip(
    tr("Show depth-passing raster samples per preview pixel"));
  p->previewViewRasterDepthPassCountAct->setCheckable(true);
  connect(p->previewViewRasterDepthPassCountAct, SIGNAL(triggered()), this,
          SLOT(setPreviewViewRasterDepthPassCount()));

  p->previewViewRasterShadeCountAct = new QAction(tr("Raster &Shade Count"), this);
  p->previewViewRasterShadeCountAct->setStatusTip(
    tr("Show shaded raster fragments per preview pixel"));
  p->previewViewRasterShadeCountAct->setCheckable(true);
  connect(p->previewViewRasterShadeCountAct, SIGNAL(triggered()), this,
          SLOT(setPreviewViewRasterShadeCount()));

  p->previewViewRasterColorWriteCountAct = new QAction(tr("Raster Color-&Write Count"), this);
  p->previewViewRasterColorWriteCountAct->setStatusTip(
    tr("Show raster color writes per preview pixel"));
  p->previewViewRasterColorWriteCountAct->setCheckable(true);
  connect(p->previewViewRasterColorWriteCountAct, SIGNAL(triggered()), this,
          SLOT(setPreviewViewRasterColorWriteCount()));

  auto previewViewGroup = new QActionGroup(this);
  previewViewGroup->addAction(p->previewViewBeautyAct);
  previewViewGroup->addAction(p->previewViewDepthAct);
  previewViewGroup->addAction(p->previewViewStencilAct);
  previewViewGroup->addAction(p->previewViewStencilCompositeAct);
  previewViewGroup->addAction(p->previewViewNormalAct);
  previewViewGroup->addAction(p->previewViewObjectIdAct);
  previewViewGroup->addAction(p->previewViewMaterialIdAct);
  previewViewGroup->addAction(p->previewViewWorldPositionAct);
  previewViewGroup->addAction(p->previewViewRasterCoverageCountAct);
  previewViewGroup->addAction(p->previewViewRasterDepthTestCountAct);
  previewViewGroup->addAction(p->previewViewRasterDepthPassCountAct);
  previewViewGroup->addAction(p->previewViewRasterShadeCountAct);
  previewViewGroup->addAction(p->previewViewRasterColorWriteCountAct);

  p->previewWireframeOverlayAct = new QAction(tr("Wireframe &Overlay"), this);
  p->previewWireframeOverlayAct->setStatusTip(
    tr("Draw graph-generated wireframe edges over the live shaded preview"));
  p->previewWireframeOverlayAct->setCheckable(true);
  connect(p->previewWireframeOverlayAct, SIGNAL(triggered(bool)), this,
          SLOT(setPreviewWireframeOverlay(bool)));

  auto previewGroup = new QActionGroup(this);
  previewGroup->addAction(p->previewRaytracerAct);
  previewGroup->addAction(p->previewScalarPathTracerAct);
  previewGroup->addAction(p->previewPathTracerAct);
  previewGroup->addAction(p->previewWavefrontAct);
  previewGroup->addAction(p->previewWireframeAct);
  previewGroup->addAction(p->previewRasterizerAct);

  p->previewTonemapLinearAct = new QAction(tr("&Linear"), this);
  p->previewTonemapLinearAct->setStatusTip(tr("Use the linear preview tonemap"));
  p->previewTonemapLinearAct->setCheckable(true);
  p->previewTonemapLinearAct->setChecked(true);
  connect(p->previewTonemapLinearAct, SIGNAL(triggered()), this, SLOT(setPreviewTonemapLinear()));

  p->previewTonemapReinhardAct = new QAction(tr("&Reinhard"), this);
  p->previewTonemapReinhardAct->setStatusTip(tr("Use the Reinhard preview tonemap"));
  p->previewTonemapReinhardAct->setCheckable(true);
  connect(p->previewTonemapReinhardAct, SIGNAL(triggered()), this,
          SLOT(setPreviewTonemapReinhard()));

  p->previewTonemapAcesAct = new QAction(tr("&ACES"), this);
  p->previewTonemapAcesAct->setStatusTip(tr("Use the ACES preview tonemap"));
  p->previewTonemapAcesAct->setCheckable(true);
  connect(p->previewTonemapAcesAct, SIGNAL(triggered()), this, SLOT(setPreviewTonemapAces()));

  auto previewTonemapGroup = new QActionGroup(this);
  previewTonemapGroup->addAction(p->previewTonemapLinearAct);
  previewTonemapGroup->addAction(p->previewTonemapReinhardAct);
  previewTonemapGroup->addAction(p->previewTonemapAcesAct);

  p->aspectStretchAct = new QAction(tr("&Stretch"), this);
  p->aspectStretchAct->setStatusTip(tr("Fill both axes independently (may distort geometry)"));
  p->aspectStretchAct->setCheckable(true);
  connect(p->aspectStretchAct, SIGNAL(triggered()), this, SLOT(setAspectStretch()));

  p->aspectFitWidthAct = new QAction(tr("Fit &Width"), this);
  p->aspectFitWidthAct->setStatusTip(
    tr("Horizontal FOV constant, vertical derived from window (square pixels, no distortion)"));
  p->aspectFitWidthAct->setCheckable(true);
  p->aspectFitWidthAct->setChecked(true);
  connect(p->aspectFitWidthAct, SIGNAL(triggered()), this, SLOT(setAspectFitWidth()));

  p->aspectFitHeightAct = new QAction(tr("Fit &Height"), this);
  p->aspectFitHeightAct->setStatusTip(
    tr("Vertical FOV constant, horizontal derived from window (square pixels, no distortion)"));
  p->aspectFitHeightAct->setCheckable(true);
  connect(p->aspectFitHeightAct, SIGNAL(triggered()), this, SLOT(setAspectFitHeight()));

  p->aspectFitExactAct = new QAction(tr("Fit &Exact (letterbox)"), this);
  p->aspectFitExactAct->setStatusTip(
    tr("Fixed intrinsic aspect ratio with black bars for the remainder"));
  p->aspectFitExactAct->setCheckable(true);
  connect(p->aspectFitExactAct, SIGNAL(triggered()), this, SLOT(setAspectFitExact()));

  auto aspectGroup = new QActionGroup(this);
  aspectGroup->addAction(p->aspectStretchAct);
  aspectGroup->addAction(p->aspectFitWidthAct);
  aspectGroup->addAction(p->aspectFitHeightAct);
  aspectGroup->addAction(p->aspectFitExactAct);

  p->aspect16x9Act = new QAction(tr("16:9"), this);
  connect(p->aspect16x9Act, SIGNAL(triggered()), this, SLOT(setAspectRatio16x9()));

  p->aspect4x3Act = new QAction(tr("4:3"), this);
  connect(p->aspect4x3Act, SIGNAL(triggered()), this, SLOT(setAspectRatio4x3()));

  p->aspect1x1Act = new QAction(tr("1:1"), this);
  connect(p->aspect1x1Act, SIGNAL(triggered()), this, SLOT(setAspectRatio1x1()));

  p->aspect239x1Act = new QAction(tr("2.39:1 (CinemaScope)"), this);
  connect(p->aspect239x1Act, SIGNAL(triggered()), this, SLOT(setAspectRatio239x1()));

  p->aspect21x9Act = new QAction(tr("21:9 (Ultrawide)"), this);
  connect(p->aspect21x9Act, SIGNAL(triggered()), this, SLOT(setAspectRatio21x9()));

  p->helpAct = new QAction(tr("Raytracer &Help"), this);
  p->helpAct->setStatusTip(tr("Go to the Github page"));
  connect(p->helpAct, SIGNAL(triggered()), this, SLOT(help()));

  auto modifyingActions = {p->newAct,    p->openAct,   p->saveAct,
                           p->saveAsAct, p->addBoxAct, p->deleteElementAct};

  for (auto& act : modifyingActions) {
    connect(act, SIGNAL(triggered()), this, SLOT(updateWindowModified()));
  }
}

void MainWindow::createMenus() {
  p->fileMenu = menuBar()->addMenu(tr("&File"));
  p->fileMenu->addAction(p->newAct);
  p->fileMenu->addAction(p->openAct);
  p->openRecentMenu = p->fileMenu->addMenu(tr("Open &Recent"));
  p->openRecentMenu->setObjectName(QStringLiteral("openRecentMenu"));
  for (auto* action : p->recentFileActs)
    p->openRecentMenu->addAction(action);
  updateRecentFileActions();
  p->fileMenu->addAction(p->importAct);
  p->fileMenu->addSeparator();
  p->fileMenu->addAction(p->saveAct);
  p->fileMenu->addAction(p->saveAsAct);

  p->editMenu = menuBar()->addMenu(tr("&Edit"));
  auto addPrimitive = p->editMenu->addMenu(tr("Add Primitive"));
  addPrimitive->addAction(p->addBoxAct);
  addPrimitive->addAction(p->addSphereAct);
  addPrimitive->addAction(p->addCylinderAct);
  addPrimitive->addAction(p->addRingAct);
  addPrimitive->addAction(p->addTorusAct);
  addPrimitive->addAction(p->addScriptAct);

  auto addSceneElement = p->editMenu->addMenu(tr("Add Scene Element"));
  addSceneElement->addAction(p->addGroupAct);

  auto addComposite = p->editMenu->addMenu(tr("Add Composite"));
  addComposite->addAction(p->addIntersectionAct);
  addComposite->addAction(p->addUnionAct);
  addComposite->addAction(p->addDifferenceAct);
  addComposite->addAction(p->addMinkowskiSumAct);
  addComposite->addAction(p->addConvexHullAct);

  auto addMaterial = p->editMenu->addMenu(tr("Add Material"));
  addMaterial->addAction(p->addMatteMaterialAct);
  addMaterial->addAction(p->addPhongMaterialAct);
  addMaterial->addAction(p->addTransparentMaterialAct);
  addMaterial->addAction(p->addReflectiveMaterialAct);

  auto addTexture = p->editMenu->addMenu(tr("Add Texture"));
  addTexture->addAction(p->addConstantColorTextureAct);
  addTexture->addAction(p->addCheckerBoardTextureAct);

  auto addLight = p->editMenu->addMenu(tr("Add Light"));
  addLight->addAction(p->addDirectionalLightAct);
  addLight->addAction(p->addPointLightAct);

  auto addCamera = p->editMenu->addMenu(tr("Add Camera"));
  addCamera->addAction(p->addPinholeCameraAct);
  addCamera->addAction(p->addFishEyeCameraAct);
  addCamera->addAction(p->addOrthographicCameraAct);
  addCamera->addAction(p->addSphericalCameraAct);
  addCamera->addAction(p->addThinLensCameraAct);
  addCamera->addAction(p->addTiltShiftCameraAct);
  addCamera->addAction(p->addEquirectangularCameraAct);

  p->editMenu->addSeparator();
  p->editMenu->addAction(p->deleteElementAct);

  p->editMenu->addSeparator();
  auto move = p->editMenu->addMenu(tr("Move"));
  move->addAction(p->moveForwardsAlongXAct);
  move->addAction(p->moveBackwardsAlongXAct);
  move->addAction(p->moveForwardsAlongYAct);
  move->addAction(p->moveBackwardsAlongYAct);
  move->addAction(p->moveForwardsAlongZAct);
  move->addAction(p->moveBackwardsAlongZAct);

  p->renderMenu = menuBar()->addMenu(tr("&Render"));
  p->renderMenu->addAction(p->renderAct);
  p->renderMenu->addSeparator();
  auto previewMenu = p->renderMenu->addMenu(tr("&Preview Engine"));
  previewMenu->addAction(p->previewUseSceneIntentAct);
  previewMenu->addSeparator();
  previewMenu->addAction(p->previewRaytracerAct);
  previewMenu->addAction(p->previewScalarPathTracerAct);
  previewMenu->addAction(p->previewPathTracerAct);
  previewMenu->addAction(p->previewWavefrontAct);
  previewMenu->addAction(p->previewWireframeAct);
  previewMenu->addAction(p->previewRasterizerAct);
  previewMenu->addSeparator();
  previewMenu->addAction(p->previewWireframeOverlayAct);
  previewMenu->addAction(p->previewRasterizerShadowsAct);
  previewMenu->addAction(p->previewFpsOverlayAct);
  previewMenu->addAction(p->previewGraphTraceCaptureAct);
  auto previewRasterBackendMenu = previewMenu->addMenu(tr("Raster &Backend"));
  previewRasterBackendMenu->addAction(p->previewRasterBackendCPUAct);
  previewRasterBackendMenu->addAction(p->previewRasterBackendOpenGLAct);
  auto previewPostAAMenu = previewMenu->addMenu(tr("Preview Post &AA"));
  previewPostAAMenu->addAction(p->previewPostAANoneAct);
  previewPostAAMenu->addAction(p->previewPostAAFxaaAct);
  previewPostAAMenu->addAction(p->previewPostAASmaaAct);

  auto previewViewMenu = previewMenu->addMenu(tr("Preview &View"));
  previewViewMenu->addAction(p->previewViewBeautyAct);
  previewViewMenu->addAction(p->previewViewDepthAct);
  previewViewMenu->addAction(p->previewViewStencilAct);
  previewViewMenu->addAction(p->previewViewStencilCompositeAct);
  previewViewMenu->addAction(p->previewViewNormalAct);
  previewViewMenu->addAction(p->previewViewObjectIdAct);
  previewViewMenu->addAction(p->previewViewMaterialIdAct);
  previewViewMenu->addAction(p->previewViewWorldPositionAct);
  previewViewMenu->addSeparator();
  previewViewMenu->addAction(p->previewViewRasterCoverageCountAct);
  previewViewMenu->addAction(p->previewViewRasterDepthTestCountAct);
  previewViewMenu->addAction(p->previewViewRasterDepthPassCountAct);
  previewViewMenu->addAction(p->previewViewRasterShadeCountAct);
  previewViewMenu->addAction(p->previewViewRasterColorWriteCountAct);

  auto previewTonemapMenu = p->renderMenu->addMenu(tr("Preview &Tonemap"));
  previewTonemapMenu->addAction(p->previewTonemapLinearAct);
  previewTonemapMenu->addAction(p->previewTonemapReinhardAct);
  previewTonemapMenu->addAction(p->previewTonemapAcesAct);

  p->renderMenu->addSeparator();
  auto aspectModeMenu = p->renderMenu->addMenu(tr("&Aspect Mode"));
  aspectModeMenu->addAction(p->aspectStretchAct);
  aspectModeMenu->addAction(p->aspectFitWidthAct);
  aspectModeMenu->addAction(p->aspectFitHeightAct);
  aspectModeMenu->addAction(p->aspectFitExactAct);

  p->aspectRatioMenu = p->renderMenu->addMenu(tr("Aspect &Ratio"));
  p->aspectRatioMenu->setEnabled(false);
  p->aspectRatioMenu->addAction(p->aspect16x9Act);
  p->aspectRatioMenu->addAction(p->aspect4x3Act);
  p->aspectRatioMenu->addAction(p->aspect1x1Act);
  p->aspectRatioMenu->addAction(p->aspect239x1Act);
  p->aspectRatioMenu->addAction(p->aspect21x9Act);

  p->helpMenu = menuBar()->addMenu(tr("&Help"));
  p->helpMenu->addAction(p->aboutAct);
  p->helpMenu->addAction(p->helpAct);
}

bool MainWindow::maybeSave() {
  if (p->scene->changed()) {
    auto response = QMessageBox::question(
      this, tr("Save changes?"),
      tr("There are unsaved changes to this document. Would you like to save them?"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);

    switch (response) {
    case QMessageBox::Save: {
      saveFile();
      return true;
    }
    case QMessageBox::Discard: {
      p->scene->setChanged(false);
      return true;
    }
    default: {
      return false;
    }
    }
  }

  return true;
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (maybeSave()) {
    if (p->mcpServer)
      p->mcpServer->stop();
    event->accept();
  } else {
    event->ignore();
  }
}

void MainWindow::newFile() {
  if (maybeSave()) {
    if (p->scene)
      delete p->scene;

    p->fileName = QString();
    p->currentElement = nullptr;
    emit selectionChanged(nullptr);

    p->scene = new ::Scene(nullptr);
    p->propertyEditorWidget->setRoot(p->scene);

    p->elementModel->setElement(p->scene);
    p->previewUseSceneIntentAct->setChecked(true);
    applySceneRenderIntentToPreviewControls();
    resetTimelineFrame();
    resetPlaybackIndex();
    redraw(PreviewCameraPolicy::ResetToSceneCamera);
  }
}

void MainWindow::reportImportDiagnostics(const world::ImportResult& result) {
  int errors = 0;
  int warnings = 0;
  for (const auto& diagnostic : result.diagnostics()) {
    if (diagnostic.isError()) {
      ++errors;
    } else {
      ++warnings;
    }
  }

  if (errors > 0) {
    statusBar()->showMessage(
      tr("Import reported %1 errors and %2 warnings").arg(errors).arg(warnings), 8000);
  } else if (warnings > 0) {
    statusBar()->showMessage(tr("Import reported %1 warnings").arg(warnings), 8000);
  }
}

QString MainWindow::openFileFilter() const {
  QStringList importPatterns = {QStringLiteral("*.json")};
  for (const QString& extension : world::SceneImporterRegistry::self().supportedExtensions()) {
    importPatterns << QStringLiteral("*.%1").arg(extension);
  }
  importPatterns.removeDuplicates();
  importPatterns.sort();
  importPatterns.removeAll(QStringLiteral("*.json"));
  importPatterns.prepend(QStringLiteral("*.json"));

  return tr("Scenes and imports (%1);;Scenes (*.json);;LDraw models (*.ldr *.dat "
            "*.mpd);;OpenSCAD models (*.scad);;All files (*)")
    .arg(importPatterns.join(QStringLiteral(" ")));
}

QString MainWindow::importFileFilter() const {
  QStringList importPatterns;
  for (const QString& extension : world::SceneImporterRegistry::self().supportedExtensions()) {
    if (extension.compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0 ||
        extension.compare(QStringLiteral("rtjson"), Qt::CaseInsensitive) == 0)
      continue;
    importPatterns << QStringLiteral("*.%1").arg(extension);
  }
  importPatterns.removeDuplicates();
  importPatterns.sort();

  if (importPatterns.isEmpty())
    return tr("All files (*)");

  return tr("Importable models (%1);;LDraw models (*.ldr *.dat *.mpd);;OpenSCAD models "
            "(*.scad);;All files (*)")
    .arg(importPatterns.join(QStringLiteral(" ")));
}

void MainWindow::loadRecentFiles() {
  p->recentFiles.load();
  updateRecentFileActions();
}

void MainWindow::addRecentFile(const QString& fileName) {
  p->recentFiles.add(fileName);
  updateRecentFileActions();
}

void MainWindow::removeRecentFile(const QString& fileName) {
  p->recentFiles.remove(fileName);
  updateRecentFileActions();
}

QString MainWindow::recentFileActionText(int index, const QString& fileName) const {
  QString displayName = QFileInfo(fileName).fileName();
  if (displayName.isEmpty())
    displayName = fileName;
  return QStringLiteral("&%1 %2").arg(index + 1).arg(displayName);
}

void MainWindow::updateRecentFileActions() {
  const int recentFileCount =
    std::min(static_cast<int>(p->recentFiles.files().size()), RecentFileList::limit);
  for (int i = 0; i != static_cast<int>(p->recentFileActs.size()); ++i) {
    QAction* action = p->recentFileActs[i];
    if (i < recentFileCount) {
      const QString fileName = p->recentFiles.files()[i];
      action->setText(recentFileActionText(i, fileName));
      action->setData(fileName);
      action->setStatusTip(QFileInfo(fileName).absoluteFilePath());
      action->setToolTip(QFileInfo(fileName).absoluteFilePath());
      action->setVisible(true);
    } else {
      action->setVisible(false);
      action->setData(QVariant());
    }
  }

  if (p->openRecentMenu)
    p->openRecentMenu->setEnabled(recentFileCount > 0);
}

void MainWindow::openFile() {
  const QString fileName =
    QFileDialog::getOpenFileName(this, tr("Open File"), QString(), openFileFilter());
  openFile(fileName);
}

void MainWindow::openRecentFile() {
  auto* action = qobject_cast<QAction*>(sender());
  if (!action)
    return;

  const QString fileName = action->data().toString();
  if (!fileName.isEmpty())
    openFile(fileName);
}

void MainWindow::openFile(const QString& fileName) {
  if (fileName.isNull() || !maybeSave())
    return;

  const QString displayName = QFileInfo(fileName).fileName();
  auto* progress = new QProgressDialog(tr("Opening %1...").arg(displayName), QString(), 0, 0, this);
  progress->setCancelButton(nullptr);
  progress->setMinimumDuration(0);
  progress->setWindowModality(Qt::WindowModal);
  progress->show();

  auto* thread = new SceneOpenThread(fileName, this);
  connect(thread, &QThread::finished, this, [this, fileName, progress, thread]() {
    OpenedScene opened = thread->takeOpenedScene();
    thread->deleteLater();
    progress->deleteLater();

    // Prune the entry from the recent list only when the file is
    // actually gone; transient or recoverable failures (parse errors,
    // unsupported features, file in use) should keep the entry so the
    // user can retry without re-locating the file.
    const bool fileMissing = !QFileInfo::exists(fileName);
    if (!opened.errorMessage.isEmpty()) {
      if (fileMissing) {
        removeRecentFile(fileName);
      }
      QMessageBox::warning(this, tr("Open File"), opened.errorMessage);
      return;
    }

    if (!opened.scene) {
      if (fileMissing) {
        removeRecentFile(fileName);
      }
      QMessageBox::warning(this, tr("Open File"), tr("Could not load %1").arg(fileName));
      return;
    }

    if (p->scene)
      delete p->scene;

    p->fileName = QString();
    p->currentElement = nullptr;
    emit selectionChanged(nullptr);

    p->scene = opened.scene.release();
    p->fileName = opened.nativeSceneFile ? fileName : QString();
    addRecentFile(fileName);
    p->propertyEditorWidget->setRoot(p->scene);
    p->elementModel->setElement(p->scene);
    p->previewUseSceneIntentAct->setChecked(true);
    applySceneRenderIntentToPreviewControls();

    resetTimelineFrame();
    resetPlaybackIndex();
    redraw(PreviewCameraPolicy::ResetToSceneCamera);
    reportImportDiagnostics(opened.importResult);
  });
  thread->start();
}

void MainWindow::importFile() {
  QString fileName =
    QFileDialog::getOpenFileName(this, tr("Import File"), QString(), importFileFilter());

  if (fileName.isNull())
    return;

  auto* progress = new QProgressDialog(tr("Importing %1...").arg(QFileInfo(fileName).fileName()),
                                       QString(), 0, 0, this);
  progress->setCancelButton(nullptr);
  progress->setMinimumDuration(0);
  progress->setWindowModality(Qt::WindowModal);
  progress->show();

  auto* thread = new SceneImportThread(fileName, this);
  connect(thread, &QThread::finished, this, [this, fileName, progress, thread]() {
    ImportedElement imported = thread->takeImportedElement();
    thread->deleteLater();
    progress->deleteLater();

    if (!imported.errorMessage.isEmpty()) {
      QMessageBox::warning(this, tr("Import File"), imported.errorMessage);
      return;
    }

    if (!imported.root) {
      QMessageBox::warning(this, tr("Import File"), tr("Could not import %1").arg(fileName));
      return;
    }

    if (auto* importedScene = qobject_cast<::Scene*>(imported.root.get())) {
      while (!importedScene->childElements().empty()) {
        p->elementModel->addElement(p->currentIndex, importedScene->childElements().front());
      }
    } else {
      p->elementModel->addElement(p->currentIndex, imported.root.release());
    }

    p->scene->resolveElementReferences();
    resetPlaybackIndex();
    elementChanged(nullptr);
    reportImportDiagnostics(imported.importResult);
  });
  thread->start();
}

void MainWindow::saveFile() {
  if (p->fileName.isNull()) {
    saveFileAs();
  } else {
    if (p->scene->save(p->fileName))
      addRecentFile(p->fileName);
  }
}

void MainWindow::saveFileAs() {
  QString fileName =
    QFileDialog::getSaveFileName(this, tr("Save File"), p->fileName, tr("Scenes (*.json)"));

  if (!fileName.isNull()) {
    p->fileName = fileName;
    if (p->scene->save(p->fileName))
      addRecentFile(p->fileName);
  }
}

template<class T>
void MainWindow::add() {
  auto element = new T(nullptr);
  element->setName(
    QString("%1 %2").arg(element->metaObject()->className()).arg(p->scene->childElements().size()));

  p->elementModel->addElement(p->currentIndex, element);
  elementChanged(element);
}

void MainWindow::addBox() {
  add<Box>();
}

void MainWindow::addSphere() {
  add<Sphere>();
}

void MainWindow::addCylinder() {
  add<Cylinder>();
}

void MainWindow::addRing() {
  add<Ring>();
}

void MainWindow::addTorus() {
  add<Torus>();
}

void MainWindow::addScript() {
  add<ScriptedSurface>();
}

void MainWindow::addGroup() {
  add<Group>();
}

void MainWindow::addIntersection() {
  add<Intersection>();
}

void MainWindow::addUnion() {
  add<Union>();
}

void MainWindow::addDifference() {
  add<Difference>();
}

void MainWindow::addMinkowskiSum() {
  add<MinkowskiSum>();
}

void MainWindow::addConvexHull() {
  add<ConvexHull>();
}

void MainWindow::addMatteMaterial() {
  add<MatteMaterial>();
}

void MainWindow::addPhongMaterial() {
  add<PhongMaterial>();
}

void MainWindow::addTransparentMaterial() {
  add<TransparentMaterial>();
}

void MainWindow::addReflectiveMaterial() {
  add<ReflectiveMaterial>();
}

void MainWindow::addConstantColorTexture() {
  add<ConstantColorTexture>();
}

void MainWindow::addCheckerBoardTexture() {
  add<CheckerBoardTexture>();
}

void MainWindow::addDirectionalLight() {
  add<DirectionalLight>();
}

void MainWindow::addPointLight() {
  add<PointLight>();
}

void MainWindow::addPinholeCamera() {
  add<PinholeCamera>();
}

void MainWindow::addFishEyeCamera() {
  add<FishEyeCamera>();
}

void MainWindow::addOrthographicCamera() {
  add<OrthographicCamera>();
}

void MainWindow::addSphericalCamera() {
  add<SphericalCamera>();
}

void MainWindow::addThinLensCamera() {
  add<ThinLensCamera>();
}

void MainWindow::addTiltShiftCamera() {
  add<TiltShiftCamera>();
}

void MainWindow::addEquirectangularCamera() {
  add<EquirectangularCamera>();
}

void MainWindow::deleteElement() {
  p->elementModel->deleteElement(p->currentIndex);
  p->currentElement = nullptr;
  p->currentIndex = QModelIndex();
  emit selectionChanged(nullptr);

  p->propertyEditorWidget->setElement(nullptr);

  p->deleteElementAct->setEnabled(false);

  elementChanged(nullptr);
}

void MainWindow::moveTransformable(const Vector3d& vec) {
  if (auto t = dynamic_cast<Transformable*>(p->currentElement)) {
    setFocus();
    t->moveBy(vec * 0.1, QGuiApplication::keyboardModifiers() & Qt::ShiftModifier);
    elementChanged(t);
  }
}

void MainWindow::moveForwardsAlongX() {
  moveTransformable(Vector3d::right());
}

void MainWindow::moveBackwardsAlongX() {
  moveTransformable(-Vector3d::right());
}

void MainWindow::moveForwardsAlongY() {
  moveTransformable(Vector3d::up());
}

void MainWindow::moveBackwardsAlongY() {
  moveTransformable(-Vector3d::up());
}

void MainWindow::moveForwardsAlongZ() {
  moveTransformable(Vector3d::forward());
}

void MainWindow::moveBackwardsAlongZ() {
  moveTransformable(-Vector3d::forward());
}

void MainWindow::render() {
  if (!p->renderWindow->isBusy()) {
    try {
      auto evaluatedScene = evaluatedSceneForCurrentFrame();
      p->renderWindow->setScene(evaluatedScene ? evaluatedScene.get() : p->scene);
      statusBar()->clearMessage();
    } catch (const std::exception& error) {
      statusBar()->showMessage(tr("Animation render failed: %1").arg(error.what()));
      return;
    }
  }

  p->renderWindow->show();
}

void MainWindow::useSceneRenderIntentPreview(bool enabled) {
  if (enabled) {
    applySceneRenderIntentToPreviewControls();
  }
  redraw();
}

void MainWindow::usePreviewRaytracer() {
  setPreviewOverrideMode();
  p->display->setEngineKind(RenderDisplay::EngineKind::Raytracer);
}

void MainWindow::usePreviewScalarPathTracer() {
  setPreviewOverrideMode();
  p->display->setEngineKind(RenderDisplay::EngineKind::ScalarPathTracer);
}

void MainWindow::usePreviewPathTracer() {
  setPreviewOverrideMode();
  p->display->setEngineKind(RenderDisplay::EngineKind::PathTracer);
}

void MainWindow::usePreviewWavefront() {
  setPreviewOverrideMode();
  p->display->setEngineKind(RenderDisplay::EngineKind::Wavefront);
}

void MainWindow::usePreviewRasterizer() {
  setPreviewOverrideMode();
  p->display->setEngineKind(RenderDisplay::EngineKind::Rasterizer);
}

void MainWindow::usePreviewWireframe() {
  setPreviewOverrideMode();
  p->display->setEngineKind(RenderDisplay::EngineKind::Wireframe);
}

void MainWindow::setPreviewRasterizerShadows(bool enabled) {
  setPreviewOverrideMode();
  if (enabled) {
    p->previewRasterizerAct->setChecked(true);
    p->display->setEngineKind(RenderDisplay::EngineKind::Rasterizer);
  }
  p->display->setRasterizerPreviewShadowsEnabled(enabled);
}

void MainWindow::setPreviewFpsOverlay(bool enabled) {
  p->display->setFpsOverlayEnabled(enabled);
}

void MainWindow::setPreviewGraphTraceCapture(bool enabled) {
  p->display->setRenderGraphTraceCaptureEnabled(enabled);
  if (p->renderGraphInspectorWidget)
    p->renderGraphInspectorWidget->setExecutionTrace(nullptr);
  redraw();
}

void MainWindow::setPreviewRasterBackendCPU() {
  setPreviewOverrideMode();
  p->display->setRasterizerPreviewBackend(engine::raster::RasterBackend::cpu());
}

void MainWindow::setPreviewRasterBackendOpenGL() {
  setPreviewOverrideMode();
  p->previewRasterizerAct->setChecked(true);
  p->display->setEngineKind(RenderDisplay::EngineKind::Rasterizer);
  p->display->setRasterizerPreviewBackend(engine::raster::RasterBackend::openGL());
}

void MainWindow::setPreviewPostAANone() {
  setPreviewOverrideMode();
  p->display->setPreviewPostProcessAA(engine::graph::RenderPostProcessAA::None);
}

void MainWindow::setPreviewPostAAFxaa() {
  setPreviewOverrideMode();
  p->display->setPreviewPostProcessAA(engine::graph::RenderPostProcessAA::FXAA);
}

void MainWindow::setPreviewPostAASmaa() {
  setPreviewOverrideMode();
  p->display->setPreviewPostProcessAA(engine::graph::RenderPostProcessAA::SMAA);
}

void MainWindow::setPreviewViewBeauty() {
  setPreviewOverrideMode();
  p->display->setPreviewViewMode(engine::graph::RenderViewMode::Beauty);
}

void MainWindow::setPreviewViewDepth() {
  setPreviewOverrideMode();
  p->display->setPreviewViewMode(engine::graph::RenderViewMode::Depth);
}

void MainWindow::setPreviewViewStencil() {
  setPreviewOverrideMode();
  p->display->setPreviewViewMode(engine::graph::RenderViewMode::Stencil);
}

void MainWindow::setPreviewViewStencilComposite() {
  setPreviewOverrideMode();
  p->previewRasterizerAct->setChecked(true);
  p->display->setEngineKind(RenderDisplay::EngineKind::Rasterizer);
  p->display->setPreviewViewMode(engine::graph::RenderViewMode::StencilComposite);
}

void MainWindow::setPreviewViewNormal() {
  setPreviewOverrideMode();
  p->display->setPreviewViewMode(engine::graph::RenderViewMode::Normal);
}

void MainWindow::setPreviewViewObjectId() {
  setPreviewOverrideMode();
  p->display->setPreviewViewMode(engine::graph::RenderViewMode::ObjectId);
}

void MainWindow::setPreviewViewMaterialId() {
  setPreviewOverrideMode();
  p->display->setPreviewViewMode(engine::graph::RenderViewMode::MaterialId);
}

void MainWindow::setPreviewViewWorldPosition() {
  setPreviewOverrideMode();
  p->display->setPreviewViewMode(engine::graph::RenderViewMode::WorldPosition);
}

void MainWindow::setPreviewViewRasterCoverageCount() {
  setPreviewRasterCounterView(engine::graph::RenderViewMode::RasterCoverageCount);
}

void MainWindow::setPreviewViewRasterDepthTestCount() {
  setPreviewRasterCounterView(engine::graph::RenderViewMode::RasterDepthTestCount);
}

void MainWindow::setPreviewViewRasterDepthPassCount() {
  setPreviewRasterCounterView(engine::graph::RenderViewMode::RasterDepthPassCount);
}

void MainWindow::setPreviewViewRasterShadeCount() {
  setPreviewRasterCounterView(engine::graph::RenderViewMode::RasterShadeCount);
}

void MainWindow::setPreviewViewRasterColorWriteCount() {
  setPreviewRasterCounterView(engine::graph::RenderViewMode::RasterColorWriteCount);
}

void MainWindow::setPreviewWireframeOverlay(bool enabled) {
  setPreviewOverrideMode();
  p->display->setWireframeOverlayEnabled(enabled);
}

void MainWindow::setPreviewTonemapLinear() {
  setPreviewTonemap("Linear");
}

void MainWindow::setPreviewTonemapReinhard() {
  setPreviewTonemap("Reinhard");
}

void MainWindow::setPreviewTonemapAces() {
  setPreviewTonemap("ACES");
}

void MainWindow::setAspectStretch() {
  p->display->setAspectMode(render::AspectMode::Stretch);
  p->aspectRatioMenu->setEnabled(false);
  p->display->render();
}

void MainWindow::setAspectFitWidth() {
  p->display->setAspectMode(render::AspectMode::FitWidth);
  p->aspectRatioMenu->setEnabled(false);
  p->display->render();
}

void MainWindow::setAspectFitHeight() {
  p->display->setAspectMode(render::AspectMode::FitHeight);
  p->aspectRatioMenu->setEnabled(false);
  p->display->render();
}

void MainWindow::setAspectFitExact() {
  p->display->setAspectMode(render::AspectMode::FitExact);
  p->aspectRatioMenu->setEnabled(true);
  p->display->render();
}

void MainWindow::setAspectRatio16x9() {
  p->display->setAspectRatio(16.0 / 9.0);
  p->aspectFitExactAct->setChecked(true);
  p->aspectRatioMenu->setEnabled(true);
  p->display->render();
}

void MainWindow::setAspectRatio4x3() {
  p->display->setAspectRatio(4.0 / 3.0);
  p->aspectFitExactAct->setChecked(true);
  p->aspectRatioMenu->setEnabled(true);
  p->display->render();
}

void MainWindow::setAspectRatio1x1() {
  p->display->setAspectRatio(1.0);
  p->aspectFitExactAct->setChecked(true);
  p->aspectRatioMenu->setEnabled(true);
  p->display->render();
}

void MainWindow::setAspectRatio239x1() {
  p->display->setAspectRatio(2.39);
  p->aspectFitExactAct->setChecked(true);
  p->aspectRatioMenu->setEnabled(true);
  p->display->render();
}

void MainWindow::setAspectRatio21x9() {
  p->display->setAspectRatio(21.0 / 9.0);
  p->aspectFitExactAct->setChecked(true);
  p->aspectRatioMenu->setEnabled(true);
  p->display->render();
}

void MainWindow::about() {
  QMessageBox::about(this, tr("About"), tr("This is the Modeler for the Raytracer library."));
}

void MainWindow::help() {
  QDesktopServices::openUrl(QUrl("https://github.com/tkadauke/raytracer"));
}

QDockWidget* MainWindow::createPropertyEditor() {
  p->propertyEditorWidget = new PropertyEditorWidget(p->scene, this);

  connect(p->propertyEditorWidget, SIGNAL(changed(Element*)), this, SLOT(elementChanged(Element*)));

  auto dockWidget = new QDockWidget("Properties", this);
  dockWidget->setWidget(p->propertyEditorWidget);

  return dockWidget;
}

QDockWidget* MainWindow::createElementSelector() {
  p->elementModel = new SceneModel(p->scene);
  auto elementTree = new QTreeView(this);
  elementTree->setDragEnabled(true);
  elementTree->setAcceptDrops(true);
  elementTree->setDropIndicatorShown(true);
  elementTree->setModel(p->elementModel);
  auto itemSelectionModel = new QItemSelectionModel(p->elementModel);
  elementTree->setSelectionModel(itemSelectionModel);
  p->itemSelectionModel = itemSelectionModel;

  connect(itemSelectionModel, SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)), this,
          SLOT(elementSelected(const QModelIndex&, const QModelIndex&)));

  connect(p->elementModel, SIGNAL(rowsMoved(const QModelIndex&, int, int, const QModelIndex&, int)),
          this, SLOT(reorder()));

  auto dockWidget = new QDockWidget("Elements", this);
  dockWidget->setWidget(elementTree);

  return dockWidget;
}

QDockWidget* MainWindow::createPreviewDisplay() {
  p->materialDisplay = new PreviewDisplayWidget(this);

  auto dockWidget = new QDockWidget("Preview", this);
  dockWidget->setWidget(p->materialDisplay);

  return dockWidget;
}

QDockWidget* MainWindow::createTimelineControls() {
  auto widget = new QWidget(this);
  auto layout = new QVBoxLayout(widget);

  auto frameLayout = new QHBoxLayout;
  auto frameLabel = new QLabel(tr("Frame"), widget);
  p->timelineFrameSlider = new QSlider(Qt::Horizontal, widget);
  p->timelineFrameSpinBox = new QSpinBox(widget);
  p->timelineSummaryLabel = new QLabel(widget);

  p->timelineFrameSpinBox->setKeyboardTracking(false);
  p->timelineFrameSpinBox->setFixedWidth(90);

  frameLayout->addWidget(frameLabel);
  frameLayout->addWidget(p->timelineFrameSlider, 1);
  frameLayout->addWidget(p->timelineFrameSpinBox);
  frameLayout->addWidget(p->timelineSummaryLabel);
  layout->addLayout(frameLayout);

  auto indexLayout = new QHBoxLayout;
  auto indexLabel = new QLabel(tr("Index"), widget);
  p->playbackIndexSlider = new QSlider(Qt::Horizontal, widget);
  p->playbackIndexSpinBox = new QSpinBox(widget);
  p->playbackSummaryLabel = new QLabel(widget);

  p->playbackIndexSpinBox->setKeyboardTracking(false);
  p->playbackIndexSpinBox->setFixedWidth(90);

  indexLayout->addWidget(indexLabel);
  indexLayout->addWidget(p->playbackIndexSlider, 1);
  indexLayout->addWidget(p->playbackIndexSpinBox);
  indexLayout->addWidget(p->playbackSummaryLabel);
  layout->addLayout(indexLayout);
  widget->setLayout(layout);

  connect(p->timelineFrameSlider, SIGNAL(valueChanged(int)), this, SLOT(setCurrentFrame(int)));
  connect(p->timelineFrameSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setCurrentFrame(int)));
  connect(p->playbackIndexSlider, SIGNAL(valueChanged(int)), this,
          SLOT(setCurrentPlaybackIndex(int)));
  connect(p->playbackIndexSpinBox, SIGNAL(valueChanged(int)), this,
          SLOT(setCurrentPlaybackIndex(int)));

  auto dockWidget = new QDockWidget("Preview Controls", this);
  dockWidget->setWidget(widget);
  p->timelineDockWidget = dockWidget;

  return dockWidget;
}

QDockWidget* MainWindow::createRenderGraphInspector() {
  p->renderGraphInspectorWidget = new RenderGraphInspectorWidget(this);

  auto dockWidget = new QDockWidget("Render Graph", this);
  dockWidget->setWidget(p->renderGraphInspectorWidget);
  p->renderGraphDockWidget = dockWidget;

  return dockWidget;
}

void MainWindow::elementChanged(Element*) {
  p->scene->setChanged(true);
  p->propertyEditorWidget->update();
  syncPlaybackControls();
  updateWindowModified();
  if (p->previewUseSceneIntentAct && p->previewUseSceneIntentAct->isChecked())
    applySceneRenderIntentToPreviewControls();
  redraw();
  emit currentElementChanged();
}

void MainWindow::elementSelected(const QModelIndex& current, const QModelIndex&) {
  auto element = static_cast<Element*>(current.internalPointer());
  p->currentElement = element;
  p->currentIndex = current;
  p->deleteElementAct->setEnabled(element != nullptr && dynamic_cast<Scene*>(element) == nullptr);

  auto transformable = dynamic_cast<Transformable*>(element);
  p->moveForwardsAlongXAct->setEnabled(transformable != nullptr);
  p->moveBackwardsAlongXAct->setEnabled(transformable != nullptr);
  p->moveForwardsAlongYAct->setEnabled(transformable != nullptr);
  p->moveBackwardsAlongYAct->setEnabled(transformable != nullptr);
  p->moveForwardsAlongZAct->setEnabled(transformable != nullptr);
  p->moveBackwardsAlongZAct->setEnabled(transformable != nullptr);

  if (element) {
    p->propertyEditorWidget->setElement(element);
  }
  if (p->centralTabs && p->display)
    p->centralTabs->setCurrentWidget(p->display);
  emit selectionChanged(element);
}

void MainWindow::updateWindowModified() {
  setWindowModified(p->scene->changed());
}

void MainWindow::updatePreviewWidget() {
  Material* mat = qobject_cast<Material*>(p->currentElement);
  Camera* cam = qobject_cast<Camera*>(p->currentElement);
  if (mat) {
    p->materialDisplay->setMaterial(mat, p->scene);
  } else if (cam) {
    p->materialDisplay->setCamera(cam, p->scene);
  } else {
    p->materialDisplay->clear();
  }
}

void MainWindow::updateRenderGraphInspector() {
  if (!p->renderGraphInspectorWidget || !p->display)
    return;

  const QSize target = p->display->bufferSize();
  const auto intent = previewRenderIntent();
  const engine::graph::RenderTargetSpec targetSpec{std::max(1, target.width()),
                                                   std::max(1, target.height()), 1};
  p->display->setRenderGraphIntent(intent);
  engine::graph::RenderGraphRequest request(intent);
  std::unique_ptr<Scene> evaluatedScene;
  const Scene* analysisScene = p->scene;
  if (p->scene) {
    try {
      evaluatedScene = evaluatedSceneForCurrentFrame();
      if (evaluatedScene) {
        analysisScene = evaluatedScene.get();
      }
    } catch (const std::exception&) {
      return;
    }
  }
  if (analysisScene) {
    auto analysis = analysisScene->renderGraphAnalysis();
    analysis.setFullGpuTracingSupportFromScene(*analysisScene->toRaytracerScene());
    request.setSceneAnalysis(analysis);
  }
  try {
    p->renderGraphInspectorWidget->setPlan(request.compile(targetSpec));
  } catch (const std::exception& error) {
    const QString message = QString::fromUtf8(error.what());
    p->renderGraphInspectorWidget->setError(message);
    p->display->setRenderGraphPreviewEnabled(false);
    statusBar()->showMessage(tr("Render graph compile failed: %1").arg(message));
    return;
  }
  applyRenderGraphPreviewPlan();
}

void MainWindow::renderGraphOverridesChanged() {
  if (applyRenderGraphPreviewPlan())
    p->display->render();
}

void MainWindow::renderGraphPassSelected(const QString& passId) {
  showRenderGraphPassDetails(passId, true);
}

void MainWindow::renderGraphResourceSelected(const QString& resourceId) {
  showRenderGraphResourceDetails(resourceId, true);
}

void MainWindow::renderGraphPassTraceChanged(const QString& passId) {
  showRenderGraphPassDetails(passId, p->centralTabs && p->centralTabs->currentWidget() ==
                                                         p->graphTracePreviewWidget);
}

void MainWindow::renderGraphResourceTraceChanged(const QString& resourceId) {
  showRenderGraphResourceDetails(resourceId, p->centralTabs && p->centralTabs->currentWidget() ==
                                                                 p->graphTracePreviewWidget);
}

void MainWindow::exportRenderGraph(const QString& format, const QByteArray& data) {
  QString extension = QStringLiteral("txt");
  QString filter = tr("Text files (*.txt)");
  if (format == QStringLiteral("dot")) {
    extension = QStringLiteral("dot");
    filter = tr("DOT files (*.dot)");
  } else if (format == QStringLiteral("json")) {
    extension = QStringLiteral("json");
    filter = tr("JSON files (*.json)");
  }

  const QString fileName = QFileDialog::getSaveFileName(
    this, tr("Export Render Graph"), QStringLiteral("render-graph.%1").arg(extension), filter);
  if (fileName.isNull())
    return;

  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    QMessageBox::warning(this, tr("Export Render Graph"), tr("Could not write %1").arg(fileName));
    return;
  }

  if (file.write(data) != data.size()) {
    QMessageBox::warning(this, tr("Export Render Graph"), tr("Could not write %1").arg(fileName));
    return;
  }

  statusBar()->showMessage(tr("Render graph exported to %1").arg(fileName));
}

void MainWindow::showRenderGraphPassDetails(const QString& passId, bool activateTracePreview) {
  if (!p->renderGraphInspectorWidget)
    return;

  const auto plan = p->renderGraphInspectorWidget->effectivePlan();
  const auto* pass = plan.findPass(passId.toStdString());
  p->propertyEditorWidget->setReadOnlyProperties(
    tr("Render graph pass"), p->renderGraphInspectorWidget->passDetailRows(passId));
  if (!pass)
    return;

  const auto trace = p->display ? p->display->lastRenderGraphExecutionTraceForPlan(plan) : nullptr;
  if (activateTracePreview && p->graphTracePreviewWidget && p->centralTabs) {
    p->graphTracePreviewWidget->showPassTrace(trace, pass->id);
    p->centralTabs->setCurrentWidget(p->graphTracePreviewWidget);
  }
}

void MainWindow::showRenderGraphResourceDetails(const QString& resourceId,
                                                bool activateTracePreview) {
  if (!p->renderGraphInspectorWidget)
    return;

  const auto plan = p->renderGraphInspectorWidget->effectivePlan();
  const auto* resource = plan.findResource(resourceId.toStdString());
  PropertyRows rows;
  if (!resource) {
    addRow(rows, tr("Resource"), resourceId);
    addRow(rows, tr("Status"), tr("not found"));
    p->propertyEditorWidget->setReadOnlyProperties(tr("Render graph resource"), rows);
    return;
  }

  addRow(rows, tr("Resource"), qstr(resource->id));
  addRow(rows, tr("Name"), qstr(resource->name));
  addRow(rows, tr("Type"), engine::graph::toString(resource->type));
  addRow(rows, tr("Format"), engine::graph::toString(resource->format));
  addRow(rows, tr("Domain"), engine::graph::toString(resource->domain));
  addRow(rows, tr("Lifetime"), engine::graph::toString(resource->lifetime));
  addRow(rows, tr("Size"), resourceSizeText(*resource));
  const bool selectorOverrideResource =
    std::find(resource->features.begin(), resource->features.end(),
              engine::graph::RenderFeatureKind("selector_override")) != resource->features.end();
  if (selectorOverrideResource) {
    addRow(rows, tr("Routing reason"), tr("Compiler-generated resource for selector route"));
  }
  QStringList featureValues;
  for (const auto& feature : resource->features)
    featureValues << qstr(feature);
  addRow(rows, tr("Features"), featureValues.join(QStringLiteral(", ")));
  addRow(rows, tr("Producer"), producerText(plan, resource->id));
  addRow(rows, tr("Consumers"), consumerText(plan, resource->id));

  const auto trace = p->display ? p->display->lastRenderGraphExecutionTraceForPlan(plan) : nullptr;
  const bool hasSnapshot = trace && trace->hasResourceSnapshots(resource->id);
  addRow(rows, tr("Trace snapshot"), hasSnapshot ? tr("available") : tr("not available"));
  const auto* cacheSnapshot = cacheSnapshotForResource(trace.get(), resource->id);
  if (cacheSnapshot) {
    addRow(rows, tr("Cache status"),
           engine::graph::toString(cacheSnapshot->cacheMetadata().status()));
    addRow(rows, tr("Cache detail"), qstr(cacheSnapshot->cacheMetadata().message()));
  } else {
    addRow(rows, tr("Cache status"), tr("not available"));
  }

  p->propertyEditorWidget->setReadOnlyProperties(tr("Render graph resource"), rows);
  if (activateTracePreview && p->graphTracePreviewWidget && p->centralTabs) {
    p->graphTracePreviewWidget->showResourceTrace(trace, resource->id);
    p->centralTabs->setCurrentWidget(p->graphTracePreviewWidget);
  }
}

void MainWindow::setCurrentFrame(int frame) {
  if (!p->scene->hasAnimation())
    return;

  const auto* timeline = p->scene->animation();
  p->currentFrame = std::clamp(frame, timeline->startFrame(), timeline->endFrame());
  syncTimelineControls();
  redraw();
}

void MainWindow::setCurrentPlaybackIndex(int index) {
  const auto range = playbackIndexRange(p->scene);
  if (!range.enabled)
    return;

  p->currentPlaybackIndex = std::clamp(index, range.first, range.last);
  p->hasPlaybackIndex = true;
  syncPlaybackControls();
  redraw();
}

void MainWindow::reorder() {
  syncPlaybackControls();
  redraw();
  p->scene->setChanged(true);

  setFocus();
  p->propertyEditorWidget->update();

  updateWindowModified();
}

void MainWindow::redraw() {
  redraw(PreviewCameraPolicy::PreserveCurrent);
}

void MainWindow::redraw(PreviewCameraPolicy cameraPolicy) {
  const auto displayCameraPolicy = cameraPolicy == PreviewCameraPolicy::ResetToSceneCamera
                                     ? RenderDisplay::CameraPolicy::ResetToSceneCamera
                                     : RenderDisplay::CameraPolicy::PreserveCurrent;
  try {
    auto evaluatedScene = evaluatedSceneForCurrentFrame();
    updateRenderGraphInspector();
    StepPlaybackStyle playbackStyle;
    if (p->hasPlaybackIndex) {
      playbackStyle.activeStep = p->currentPlaybackIndex;
    }
    p->display->setScene(evaluatedScene ? evaluatedScene.get() : p->scene, playbackStyle,
                         displayCameraPolicy);
    statusBar()->clearMessage();
  } catch (const std::exception& error) {
    statusBar()->showMessage(tr("Preview update failed: %1").arg(error.what()));
    p->display->setScene(p->scene, StepPlaybackStyle(), displayCameraPolicy);
  }
}

bool MainWindow::applyRenderGraphPreviewPlan() {
  if (!p->renderGraphInspectorWidget || !p->display)
    return false;

  const auto plan = p->renderGraphInspectorWidget->effectivePlan();
  const auto validation = plan.validate();
  if (!validation.valid()) {
    const auto& first = validation.errors().front();
    p->display->setRenderGraphPreviewEnabled(false);
    statusBar()->showMessage(
      tr("Render graph preview paused: %1").arg(QString::fromStdString(first.message)));
    return false;
  }

  p->display->setRenderGraphPlan(plan);
  statusBar()->clearMessage();
  return true;
}

void MainWindow::setPreviewTonemap(const std::string& name) {
  auto tonemap = render::TonemapFactory::self().createShared(name);
  if (!tonemap) {
    statusBar()->showMessage(
      tr("Preview tonemap is not registered: %1").arg(QString::fromStdString(name)));
    return;
  }

  p->display->setPreviewTonemap(std::move(tonemap));
}

void MainWindow::setPreviewRasterCounterView(engine::graph::RenderViewMode viewMode) {
  setPreviewOverrideMode();
  p->previewRasterizerAct->setChecked(true);
  p->display->setEngineKind(RenderDisplay::EngineKind::Rasterizer);
  p->display->setPreviewViewMode(viewMode);
}

void MainWindow::setPreviewOverrideMode() {
  if (p->previewUseSceneIntentAct)
    p->previewUseSceneIntentAct->setChecked(false);
}

void MainWindow::applySceneRenderIntentToPreviewControls() {
  if (!p->scene || !p->display)
    return;

  const auto intent =
    p->scene->hasRenderIntent() ? p->scene->renderIntent() : engine::graph::RenderIntent();
  const auto rayIntegrator = intent.engineOptions.raytracer().integrator();
  const bool scalarPathTracerIntent =
    intent.defaultExecutor == engine::graph::RenderExecutorPreference::Raytracer && rayIntegrator &&
    (*rayIntegrator == "pathtracer" || *rayIntegrator == "path_tracer" || *rayIntegrator == "pt");
  struct EngineChoice {
    engine::graph::RenderExecutorPreference preference;
    RenderDisplay::EngineKind kind;
    QAction* action;
  };
  const std::vector<EngineChoice> engines = {
    {engine::graph::RenderExecutorPreference::Raytracer, RenderDisplay::EngineKind::Raytracer,
     p->previewRaytracerAct},
    {engine::graph::RenderExecutorPreference::PathTracer, RenderDisplay::EngineKind::PathTracer,
     p->previewPathTracerAct},
    {engine::graph::RenderExecutorPreference::Wavefront, RenderDisplay::EngineKind::Wavefront,
     p->previewWavefrontAct},
    {engine::graph::RenderExecutorPreference::Rasterizer, RenderDisplay::EngineKind::Rasterizer,
     p->previewRasterizerAct},
    {engine::graph::RenderExecutorPreference::Wireframe, RenderDisplay::EngineKind::Wireframe,
     p->previewWireframeAct},
  };
  const auto engine = std::find_if(engines.begin(), engines.end(), [&](const EngineChoice& choice) {
    return choice.preference == intent.defaultExecutor;
  });
  if (scalarPathTracerIntent) {
    p->previewScalarPathTracerAct->setChecked(true);
    p->display->setEngineKind(RenderDisplay::EngineKind::ScalarPathTracer);
  } else if (engine != engines.end()) {
    engine->action->setChecked(true);
    p->display->setEngineKind(engine->kind);
  }

  struct ViewChoice {
    engine::graph::RenderViewMode viewMode;
    QAction* action;
  };
  const std::vector<ViewChoice> views = {
    {engine::graph::RenderViewMode::Default, p->previewViewBeautyAct},
    {engine::graph::RenderViewMode::Beauty, p->previewViewBeautyAct},
    {engine::graph::RenderViewMode::Depth, p->previewViewDepthAct},
    {engine::graph::RenderViewMode::Stencil, p->previewViewStencilAct},
    {engine::graph::RenderViewMode::StencilComposite, p->previewViewStencilCompositeAct},
    {engine::graph::RenderViewMode::Normal, p->previewViewNormalAct},
    {engine::graph::RenderViewMode::ObjectId, p->previewViewObjectIdAct},
    {engine::graph::RenderViewMode::MaterialId, p->previewViewMaterialIdAct},
    {engine::graph::RenderViewMode::WorldPosition, p->previewViewWorldPositionAct},
    {engine::graph::RenderViewMode::RasterCoverageCount, p->previewViewRasterCoverageCountAct},
    {engine::graph::RenderViewMode::RasterDepthTestCount, p->previewViewRasterDepthTestCountAct},
    {engine::graph::RenderViewMode::RasterDepthPassCount, p->previewViewRasterDepthPassCountAct},
    {engine::graph::RenderViewMode::RasterShadeCount, p->previewViewRasterShadeCountAct},
    {engine::graph::RenderViewMode::RasterColorWriteCount, p->previewViewRasterColorWriteCountAct},
  };
  const auto view = std::find_if(views.begin(), views.end(), [&](const ViewChoice& choice) {
    return choice.viewMode == intent.defaultViewMode;
  });
  if (view != views.end()) {
    view->action->setChecked(true);
    const auto viewMode = intent.defaultViewMode == engine::graph::RenderViewMode::Default
                            ? engine::graph::RenderViewMode::Beauty
                            : intent.defaultViewMode;
    p->display->setPreviewViewMode(viewMode);
  }

  struct PostAAChoice {
    engine::graph::RenderPostProcessAA mode;
    QAction* action;
  };
  const std::vector<PostAAChoice> aaModes = {
    {engine::graph::RenderPostProcessAA::None, p->previewPostAANoneAct},
    {engine::graph::RenderPostProcessAA::FXAA, p->previewPostAAFxaaAct},
    {engine::graph::RenderPostProcessAA::SMAA, p->previewPostAASmaaAct},
  };
  const auto aaMode = std::find_if(aaModes.begin(), aaModes.end(), [&](const PostAAChoice& choice) {
    return choice.mode == intent.postProcessAA;
  });
  if (aaMode != aaModes.end()) {
    aaMode->action->setChecked(true);
    p->display->setPreviewPostProcessAA(aaMode->mode);
  }

  p->previewRasterizerShadowsAct->setChecked(intent.enablePreviewShadows);
  p->display->setRasterizerPreviewShadowsEnabled(intent.enablePreviewShadows);
  const auto backend =
    intent.engineOptions.rasterizer().backend().value_or(engine::raster::RasterBackend::cpu());
  if (backend.isOpenGL()) {
    p->previewRasterBackendOpenGLAct->setChecked(true);
  } else {
    p->previewRasterBackendCPUAct->setChecked(true);
  }
  p->display->setRasterizerPreviewBackend(backend);
  p->previewWireframeOverlayAct->setChecked(intent.enableWireframeOverlay);
  p->display->setWireframeOverlayEnabled(intent.enableWireframeOverlay);
}

void MainWindow::resetTimelineFrame() {
  if (const auto* timeline = p->scene->animation()) {
    p->currentFrame = timeline->startFrame();
  } else {
    p->currentFrame = 0;
  }
  syncTimelineControls();
}

void MainWindow::syncTimelineControls() {
  const auto* timeline = p->scene->animation();
  const bool hasAnimation = timeline != nullptr;

  p->timelineDockWidget->setEnabled(hasAnimation);
  {
    const QSignalBlocker sliderBlocker(p->timelineFrameSlider);
    const QSignalBlocker spinBoxBlocker(p->timelineFrameSpinBox);

    if (hasAnimation) {
      p->currentFrame = std::clamp(p->currentFrame, timeline->startFrame(), timeline->endFrame());
      p->timelineFrameSlider->setRange(timeline->startFrame(), timeline->endFrame());
      p->timelineFrameSpinBox->setRange(timeline->startFrame(), timeline->endFrame());
      p->timelineFrameSlider->setValue(p->currentFrame);
      p->timelineFrameSpinBox->setValue(p->currentFrame);
    } else {
      p->timelineFrameSlider->setRange(0, 0);
      p->timelineFrameSpinBox->setRange(0, 0);
      p->timelineFrameSlider->setValue(0);
      p->timelineFrameSpinBox->setValue(0);
    }
  }

  if (hasAnimation) {
    p->timelineSummaryLabel->setText(tr("%1-%2, %3 fps")
                                       .arg(timeline->startFrame())
                                       .arg(timeline->endFrame())
                                       .arg(timeline->fps()));
  } else {
    p->timelineSummaryLabel->setText(tr("No animation"));
  }
}

void MainWindow::resetPlaybackIndex() {
  const auto range = playbackIndexRange(p->scene);
  p->hasPlaybackIndex = range.enabled;
  p->currentPlaybackIndex = range.enabled ? range.first : 0;
  syncPlaybackControls();
}

void MainWindow::syncPlaybackControls() {
  const auto range = playbackIndexRange(p->scene);
  p->hasPlaybackIndex = range.enabled;

  p->timelineDockWidget->setEnabled(p->scene->hasAnimation() || range.enabled);
  {
    const QSignalBlocker sliderBlocker(p->playbackIndexSlider);
    const QSignalBlocker spinBoxBlocker(p->playbackIndexSpinBox);

    p->playbackIndexSlider->setEnabled(range.enabled);
    p->playbackIndexSpinBox->setEnabled(range.enabled);

    if (range.enabled) {
      p->currentPlaybackIndex = std::clamp(p->currentPlaybackIndex, range.first, range.last);
      p->playbackIndexSlider->setRange(range.first, range.last);
      p->playbackIndexSpinBox->setRange(range.first, range.last);
      p->playbackIndexSlider->setValue(p->currentPlaybackIndex);
      p->playbackIndexSpinBox->setValue(p->currentPlaybackIndex);
    } else {
      p->currentPlaybackIndex = 0;
      p->playbackIndexSlider->setRange(0, 0);
      p->playbackIndexSpinBox->setRange(0, 0);
      p->playbackIndexSlider->setValue(0);
      p->playbackIndexSpinBox->setValue(0);
    }
  }

  if (range.enabled) {
    p->playbackSummaryLabel->setText(
      tr("%1-%2, %3 indexed group(s)").arg(range.first).arg(range.last).arg(range.count));
  } else {
    p->playbackSummaryLabel->setText(tr("No indexed groups"));
  }
}

engine::graph::RenderIntent MainWindow::previewRenderIntent() const {
  engine::graph::RenderGraphRequest request(
    p->scene ? p->scene->renderIntentWithActiveCameraDefault() : engine::graph::RenderIntent());

  if (!p->display)
    return request.resolvedIntent();

  if (p->previewUseSceneIntentAct && p->previewUseSceneIntentAct->isChecked())
    return request.resolvedIntent();

  request.setPreviewShadowsOverride(p->display->rasterizerPreviewShadowsEnabled())
    .setRasterBackendOverride(p->display->rasterizerPreviewBackend())
    .setPostProcessAAOverride(p->display->previewPostProcessAA())
    .setWireframeOverlayOverride(p->display->wireframeOverlayEnabled());

  const engine::graph::RenderViewMode previewViewMode = p->display->previewViewMode();

  previewEngineIntentDefinition(p->display->engineKind()).apply(request, previewViewMode);

  return request.resolvedIntent();
}

std::unique_ptr<Scene> MainWindow::evaluatedSceneForCurrentFrame() const {
  if (!p->scene->hasAnimation())
    return nullptr;
  return p->scene->evaluatedAtFrame(p->currentFrame);
}
