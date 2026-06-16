#include "widgets/world/RenderWindow.h"
#include "widgets/world/RenderGraphInspectorWidget.h"
#include "widgets/world/RenderSettingsWidget.h"

#include "widgets/RenderWidget.h"

#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/RenderEngineOptions.h"
#include "engine/graph/RenderGraphRequest.h"
#include "engine/raster/RasterBackend.h"
#include "render/primitives/Scene.h"
#include "render/cameras/Camera.h"

#include "world/objects/Scene.h"
#include "world/objects/Camera.h"

#include <QElapsedTimer>
#include <QGridLayout>
#include <QScrollArea>
#include <QTabWidget>

#include <exception>

struct RenderWindow::Private {
  inline Private()
      : renderWidget(nullptr),
        settingsWidget(nullptr),
        graphInspector(nullptr),
        busy(false),
        timer(0) {
  }

  RenderWidget* renderWidget;
  RenderSettingsWidget* settingsWidget;
  RenderGraphInspectorWidget* graphInspector;

  std::shared_ptr<engine::graph::GraphRenderEngine> graph;
  engine::graph::RenderSceneAnalysis sceneAnalysis{
    engine::graph::RenderSceneAnalysis::unknownScene()};
  engine::graph::RenderIntent baseIntent;

  void bindSceneCameras(const Scene& scene) {
    graph->clearSceneCameras();
    for (const Camera* camera : scene.cameras()) {
      if (!camera->id().isEmpty()) {
        graph->setSceneCamera(camera->id().toStdString(), camera->toRaytracer());
      }
    }
  }

  engine::graph::RenderPostProcessAA postProcessAA() const {
    const QString postAA = settingsWidget->postProcessAA();
    if (postAA == "FXAA") {
      return engine::graph::RenderPostProcessAA::FXAA;
    }
    if (postAA == "SMAA") {
      return engine::graph::RenderPostProcessAA::SMAA;
    }
    if (postAA == "TAA") {
      return engine::graph::RenderPostProcessAA::TAA;
    }
    return engine::graph::RenderPostProcessAA::None;
  }

  void applyRayDenoiserOverride(engine::graph::RenderRaytracerOptions& options) const {
    if (!settingsWidget->denoiserOverrideEnabled()) {
      return;
    }

    const QString denoiser = settingsWidget->denoiser();
    if (denoiser == "None") {
      options.setDenoiser("none");
    } else if (denoiser == "Box") {
      options.setDenoiser("box");
      options.setDenoiseRadius(settingsWidget->denoiseRadius());
    } else if (denoiser == "Bilateral") {
      options.setDenoiser("bilateral");
      options.setDenoiseRadius(settingsWidget->denoiseRadius());
      options.setDenoiseColorSigma(settingsWidget->denoiseColorSigma());
    }
  }

  engine::graph::RenderIntent renderIntent() const {
    engine::graph::RenderIntent intent = baseIntent;

    if (settingsWidget->engine() == "Rasterizer") {
      intent.defaultExecutor = engine::graph::RenderExecutorPreference::Rasterizer;
      intent.enablePreviewShadows = settingsWidget->shadowMapsEnabled();
      intent.postProcessAA = postProcessAA();
      auto& options = intent.engineOptions.rasterizer();
      options.setBackend(engine::raster::RasterBackend::fromString(
        settingsWidget->rasterBackend().toStdString(), "render dialog raster backend"));
      options.setLod(settingsWidget->lod());
      options.setMSAASamples(settingsWidget->msaaSamples());
      if (settingsWidget->msaaShadingMode() == "Per fragment") {
        options.setMSAAShadingMode("per_fragment");
      }
      options.setMaximumThreads(settingsWidget->renderThreads());
      if (settingsWidget->shadowMapsEnabled()) {
        options.setShadowMapSize(settingsWidget->shadowMapSize());
        options.setShadowCascadeCount(settingsWidget->shadowCascadeCount());
        options.setShadowCascadeSplitLambda(settingsWidget->shadowCascadeSplitLambda());
        options.setShadowBias(settingsWidget->shadowBias());
        options.setShadowSlopeBias(settingsWidget->shadowSlopeBias());
        options.setShadowFilterRadius(settingsWidget->shadowFilterRadius());
        if (settingsWidget->shadowFilterMode() == "PCSS") {
          options.setShadowFilterMode("pcss");
        }
      }
    } else if (settingsWidget->engine() == "Wireframe") {
      intent.defaultExecutor = engine::graph::RenderExecutorPreference::Wireframe;
      intent.engineOptions.wireframe().setLod(settingsWidget->lod());
    } else {
      if (settingsWidget->engine() == "Path Tracer") {
        intent.defaultExecutor = settingsWidget->pathTracingSchedule() == "Scalar"
                                   ? engine::graph::RenderExecutorPreference::Raytracer
                                   : engine::graph::RenderExecutorPreference::PathTracer;
      } else {
        intent.defaultExecutor = engine::graph::RenderExecutorPreference::Raytracer;
      }
      auto& options = intent.engineOptions.raytracer();
      if (settingsWidget->engine() == "Path Tracer") {
        options.setIntegrator("pathtracer");
      } else {
        options.setIntegrator("whitted");
      }
      options.setSampler(settingsWidget->sampler().toStdString());
      options.setSamplesPerPixel(settingsWidget->samplesPerPixel());
      options.setViewPlane(settingsWidget->viewPlane().toStdString());
      options.setMaximumRecursionDepth(settingsWidget->maxRecursionDepth());
      if (settingsWidget->engine() == "Path Tracer") {
        options.setDirectLightSamples(settingsWidget->directLightSamples());
        options.setTracingExecution(settingsWidget->tracingExecution().toLower().toStdString());
        if (settingsWidget->tracingExecution() == "CPU") {
          options.setIntersectionBackend("cpu");
        } else if (settingsWidget->tracingExecution() == "GPU") {
          options.setIntersectionBackend("gpu");
        } else if (settingsWidget->pathTracingSchedule() == "Wavefront" &&
                   settingsWidget->tracingExecution() == "Hybrid") {
          options.setIntersectionBackend(
            settingsWidget->wavefrontIntersectionBackend().toStdString());
        }
      }
      options.setMaximumThreads(settingsWidget->renderThreads());
      options.setQueueSize(settingsWidget->queueSize());
      applyRayDenoiserOverride(options);
    }
    return intent;
  }

  int fallbackSampleCount(const engine::graph::RenderIntent& intent) const {
    if (intent.defaultExecutorKind() == engine::graph::RenderExecutorKind::Rasterizer)
      return settingsWidget->msaaSamples();
    if (intent.defaultExecutorKind() == engine::graph::RenderExecutorKind::Raytracer ||
        intent.defaultExecutorKind() == engine::graph::RenderExecutorKind::Wavefront)
      return settingsWidget->samplesPerPixel();
    return 1;
  }

  engine::graph::RenderPlan compilePlan() const {
    const QSize resolution = settingsWidget->resolution();
    const auto intent = renderIntent();
    engine::graph::RenderGraphRequest request(intent);
    request.setSceneAnalysis(sceneAnalysis);
    return request.compile({resolution.width(), resolution.height(),
                            intent.targetSampleCountHint(fallbackSampleCount(intent))});
  }

  void updateGraphPreview() {
    if (!graphInspector)
      return;

    try {
      graphInspector->setPlan(compilePlan());
    } catch (const std::exception& error) {
      graphInspector->setError(QString::fromUtf8(error.what()));
    }
  }

  bool busy;
  int timer;
  QElapsedTimer time;
};

RenderWindow::RenderWindow(QWidget* parent)
    : QWidget(parent),
      p(std::make_unique<Private>()) {
  p->graph = std::make_shared<engine::graph::GraphRenderEngine>(nullptr);
  p->graph->setExecutionTraceEnabled(true);

  auto grid = new QGridLayout(this);
  p->settingsWidget = new RenderSettingsWidget(this);

  auto tabs = new QTabWidget(this);
  auto scrollArea = new QScrollArea(this);
  p->renderWidget = new RenderWidget(scrollArea, p->graph);
  scrollArea->setWidget(p->renderWidget);
  tabs->addTab(scrollArea, tr("Image"));

  p->graphInspector = new RenderGraphInspectorWidget(this);
  tabs->addTab(p->graphInspector, tr("Graph"));

  grid->addWidget(p->settingsWidget, 0, 0);
  grid->addWidget(tabs, 0, 1);

  setLayout(grid);

  connect(p->settingsWidget, SIGNAL(renderClicked()), this, SLOT(render()));
  connect(p->settingsWidget, SIGNAL(stopClicked()), this, SLOT(stop()));
  connect(p->settingsWidget, &RenderSettingsWidget::settingsChanged, this,
          [this] { p->updateGraphPreview(); });

  connect(p->renderWidget, SIGNAL(finished()), this, SLOT(finished()));
  connect(p->renderWidget, &RenderWidget::renderFailed, this,
          [this](const QString&) { finished(); });
}

// Defined here so the unique_ptr<Private> deleter sees the complete Private
// type — Private is forward-declared in the header for pimpl.
RenderWindow::~RenderWindow() = default;

QSize RenderWindow::sizeHint() const {
  return QSize(1024, 768);
}

void RenderWindow::timerEvent(QTimerEvent*) {
  if (isBusy()) {
    p->settingsWidget->setElapsedTime(p->time.elapsed());
  }
}

bool RenderWindow::isBusy() const {
  return p->busy;
}

void RenderWindow::render() {
  p->busy = true;
  p->time.restart();

  p->settingsWidget->setBusy(true);
  p->graphInspector->setExecutionTrace(nullptr);
  p->graphInspector->clearExecutionState();

  p->renderWidget->resize(p->settingsWidget->resolution());
  p->renderWidget->setBufferSize(p->settingsWidget->resolution());

  const auto intent = p->renderIntent();
  p->graph->setIntent(intent);
  if (p->graphInspector->effectivePlanValid()) {
    p->graph->setPlan(p->graphInspector->effectivePlan());
  } else {
    p->graph->setPlan(p->compilePlan());
  }
  p->renderWidget->setEngine(p->graph);
  p->renderWidget->setDisplayMode(p->settingsWidget->displayMode());
  p->renderWidget->setShowProgressIndicators(p->settingsWidget->showProgressIndicators());

  p->renderWidget->render();

  p->timer = startTimer(1000);
}

void RenderWindow::stop() {
  p->renderWidget->stop();
  finished();
}

void RenderWindow::finished() {
  p->settingsWidget->setBusy(false);
  p->busy = false;
  p->graphInspector->setExecutionTrace(
    p->graph->lastExecutionTraceForPlan(p->graphInspector->effectivePlan()));
}

void RenderWindow::setScene(::Scene* scene) {
  auto raytracerScene = scene->toRaytracerScene();
  p->sceneAnalysis = scene->renderGraphAnalysis();
  p->baseIntent = scene->renderIntentWithActiveCameraDefault();
  p->settingsWidget->setRenderIntent(p->baseIntent);

  p->graph->setScene(raytracerScene);
  p->graph->setSceneAnalysis(p->sceneAnalysis);
  p->bindSceneCameras(*scene);

  auto camera = scene->activeCamera();
  std::shared_ptr<render::Camera> rtCamera;
  if (camera) {
    rtCamera = camera->toRaytracer();
  } else {
    rtCamera = p->graph->camera();
    rtCamera->setPosition(Matrix3d::rotateY(-25_degrees) * Matrix3d::rotateX(-25_degrees) *
                          Vector3d(0, 0, -5));
  }
  p->graph->setCamera(rtCamera);
  p->updateGraphPreview();
}
