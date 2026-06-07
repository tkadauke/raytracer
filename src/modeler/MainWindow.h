#pragma once

#include "core/math/Vector.h"
#include "engine/graph/RenderGraphTypes.h"
#include <QByteArray>
#include <QMainWindow>
#include <QString>
#include <memory>
#include <string>

class QDockWidget;

class PropertyEditorWidget;
class PreviewDisplayWidget;
class RenderGraphInspectorWidget;
class RenderDisplay;
class SceneModel;

class Element;
class Scene;

class RenderWindow;

namespace world {
  class ImportResult;
}

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow();
  ~MainWindow();

protected:
  virtual void closeEvent(QCloseEvent* event);

signals:
  void selectionChanged(Element* element);
  void currentElementChanged();

private slots:
  void elementSelected(const QModelIndex& current, const QModelIndex& previous);
  void elementChanged(Element*);
  void updateWindowModified();
  void updatePreviewWidget();
  void updateRenderGraphInspector();
  void renderGraphOverridesChanged();
  void renderGraphPassSelected(const QString& passId);
  void renderGraphResourceSelected(const QString& resourceId);
  void renderGraphPassTraceChanged(const QString& passId);
  void renderGraphResourceTraceChanged(const QString& resourceId);
  void exportRenderGraph(const QString& format, const QByteArray& data);
  void setCurrentFrame(int frame);
  void setCurrentPlaybackIndex(int index);

  void newFile();
  void openFile();
  void openRecentFile();
  void importFile();
  void saveFile();
  void saveFileAs();

  void addBox();
  void addSphere();
  void addCylinder();
  void addRing();
  void addTorus();
  void addScript();
  void addGroup();

  void addIntersection();
  void addUnion();
  void addDifference();
  void addMinkowskiSum();
  void addConvexHull();

  void addMatteMaterial();
  void addPhongMaterial();
  void addTransparentMaterial();
  void addReflectiveMaterial();

  void addConstantColorTexture();
  void addCheckerBoardTexture();

  void addDirectionalLight();
  void addPointLight();

  void addPinholeCamera();
  void addFishEyeCamera();
  void addOrthographicCamera();
  void addSphericalCamera();
  void addThinLensCamera();
  void addTiltShiftCamera();
  void addEquirectangularCamera();

  void deleteElement();

  void moveForwardsAlongX();
  void moveBackwardsAlongX();
  void moveForwardsAlongY();
  void moveBackwardsAlongY();
  void moveForwardsAlongZ();
  void moveBackwardsAlongZ();

  void render();
  void useSceneRenderIntentPreview(bool enabled);
  void usePreviewRaytracer();
  void usePreviewPathTracer();
  void usePreviewWavefront();
  void usePreviewWireframe();
  void usePreviewRasterizer();
  void setPreviewRasterizerShadows(bool enabled);
  void setPreviewFpsOverlay(bool enabled);
  void setPreviewRasterBackendCPU();
  void setPreviewRasterBackendOpenGL();
  void setPreviewPostAANone();
  void setPreviewPostAAFxaa();
  void setPreviewPostAASmaa();
  void setPreviewViewBeauty();
  void setPreviewViewDepth();
  void setPreviewViewStencil();
  void setPreviewViewStencilComposite();
  void setPreviewViewNormal();
  void setPreviewViewObjectId();
  void setPreviewViewMaterialId();
  void setPreviewViewWorldPosition();
  void setPreviewViewRasterCoverageCount();
  void setPreviewViewRasterDepthTestCount();
  void setPreviewViewRasterDepthPassCount();
  void setPreviewViewRasterShadeCount();
  void setPreviewViewRasterColorWriteCount();
  void setPreviewWireframeOverlay(bool enabled);
  void setPreviewTonemapLinear();
  void setPreviewTonemapReinhard();
  void setPreviewTonemapAces();

  void setAspectStretch();
  void setAspectFitWidth();
  void setAspectFitHeight();
  void setAspectFitExact();
  void setAspectRatio16x9();
  void setAspectRatio4x3();
  void setAspectRatio1x1();
  void setAspectRatio239x1();
  void setAspectRatio21x9();

  void about();
  void help();

  void reorder();

private:
  enum class PreviewCameraPolicy { ResetToSceneCamera, PreserveCurrent };

  void reportImportDiagnostics(const world::ImportResult& result);
  [[nodiscard]] QString openFileFilter() const;
  [[nodiscard]] QString importFileFilter() const;

  QDockWidget* createPropertyEditor();
  QDockWidget* createElementSelector();
  QDockWidget* createPreviewDisplay();
  QDockWidget* createTimelineControls();
  QDockWidget* createRenderGraphInspector();

  void createActions();
  void createMenus();

  bool maybeSave();
  void openFile(const QString& fileName);
  void loadRecentFiles();
  void addRecentFile(const QString& fileName);
  void removeRecentFile(const QString& fileName);
  void updateRecentFileActions();
  [[nodiscard]] QString recentFileActionText(int index, const QString& fileName) const;

  void redraw();
  void redraw(PreviewCameraPolicy cameraPolicy);
  bool applyRenderGraphPreviewPlan();
  void showRenderGraphPassDetails(const QString& passId, bool activateTracePreview);
  void showRenderGraphResourceDetails(const QString& resourceId, bool activateTracePreview);
  void setPreviewTonemap(const std::string& name);
  void setPreviewRasterCounterView(engine::graph::RenderViewMode viewMode);
  void setPreviewOverrideMode();
  void applySceneRenderIntentToPreviewControls();
  void resetTimelineFrame();
  void syncTimelineControls();
  void resetPlaybackIndex();
  void syncPlaybackControls();
  engine::graph::RenderIntent previewRenderIntent() const;
  std::unique_ptr<Scene> evaluatedSceneForCurrentFrame() const;

  template<class T>
  void add();

  void moveTransformable(const Vector3d& vec);

  struct Private;
  std::unique_ptr<Private> p;
};
