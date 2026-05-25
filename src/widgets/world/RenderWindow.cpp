#include "widgets/world/RenderWindow.h"
#include "widgets/world/RenderSettingsWidget.h"

#include "widgets/RenderWidget.h"

#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/RasterPassState.h"
#include "engine/graph/RenderGraphCompiler.h"
#include "engine/raster/Rasterizer.h"
#include "engine/raytracer/Raytracer.h"
#include "engine/wireframe/Wireframe.h"
#include "render/lights/PointLight.h"
#include "render/primitives/Scene.h"
#include "render/cameras/Camera.h"

#include "render/samplers/SamplerFactory.h"
#include "render/viewplanes/ViewPlaneFactory.h"

#include "world/objects/Scene.h"
#include "world/objects/Camera.h"

#include <QGridLayout>
#include <QScrollArea>
#include <QElapsedTimer>

using namespace engine::raytracer;

struct RenderWindow::Private {
  inline Private()
      : renderWidget(nullptr),
        settingsWidget(nullptr),
        busy(false),
        timer(0) {
  }

  RenderWidget* renderWidget;
  RenderSettingsWidget* settingsWidget;

  // Per-engine instances kept around so swapping is cheap. Only one
  // is wired into renderWidget at a time; the other holds onto its
  // scene + camera ready to take over on the next "Render" click.
  std::shared_ptr<engine::raytracer::Raytracer> raytracer;
  std::shared_ptr<engine::wireframe::Wireframe> wireframe;
  std::shared_ptr<engine::graph::GraphRenderEngine> rasterGraph;

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

  engine::graph::RenderIntent rasterIntent() const {
    engine::graph::RenderIntent intent;
    intent.defaultExecutor = engine::graph::RenderExecutorPreference::Rasterizer;
    intent.defaultViewMode = engine::graph::RenderViewMode::Beauty;
    intent.enablePreviewShadows = settingsWidget->shadowMapsEnabled();
    intent.postProcessAA = postProcessAA();
    return intent;
  }

  engine::graph::RasterBeautyPassState rasterBeautyPassState(engine::graph::RenderPostProcessAA aa,
                                                             bool includeImagePostProcessAA,
                                                             bool includeShadowMapEnable) const {
    engine::graph::RasterBeautyPassState state;
    state.geometry().setLod(settingsWidget->lod());
    state.sampling().setMSAASamples(settingsWidget->msaaSamples());
    if (settingsWidget->msaaShadingMode() == "Per fragment") {
      state.sampling().setMSAAShadingMode(engine::raster::Rasterizer::MSAAShadingMode::PerFragment);
    }
    if (includeImagePostProcessAA && aa == engine::graph::RenderPostProcessAA::FXAA) {
      state.sampling().setPostProcessAA(engine::raster::Rasterizer::PostProcessAA::FXAA);
    } else if (includeImagePostProcessAA && aa == engine::graph::RenderPostProcessAA::SMAA) {
      state.sampling().setPostProcessAA(engine::raster::Rasterizer::PostProcessAA::SMAA);
    } else if (aa == engine::graph::RenderPostProcessAA::TAA) {
      state.sampling().setPostProcessAA(engine::raster::Rasterizer::PostProcessAA::TAA);
    }

    state.execution().setMaximumThreads(settingsWidget->renderThreads());
    if (includeShadowMapEnable) {
      state.shadows().setShadowMapsEnabled(settingsWidget->shadowMapsEnabled());
      state.shadows().setShadowMapSize(settingsWidget->shadowMapSize());
      state.shadows().setShadowCascadeCount(settingsWidget->shadowCascadeCount());
      state.shadows().setShadowCascadeSplitLambda(settingsWidget->shadowCascadeSplitLambda());
      state.shadows().setShadowBias(settingsWidget->shadowBias());
      state.shadows().setShadowSlopeBias(settingsWidget->shadowSlopeBias());
      state.shadows().setShadowFilterRadius(settingsWidget->shadowFilterRadius());
      if (settingsWidget->shadowFilterMode() == "PCSS") {
        state.shadows().setShadowFilterMode(engine::raster::Rasterizer::ShadowFilterMode::PCSS);
      }
    }
    return state;
  }

  engine::graph::RasterShadowPassState rasterShadowPassState() const {
    engine::graph::RasterShadowPassState state;
    state.shadows().setShadowMapsEnabled(true);
    state.shadows().setShadowMapSize(settingsWidget->shadowMapSize());
    state.shadows().setShadowCascadeCount(settingsWidget->shadowCascadeCount());
    state.shadows().setShadowCascadeSplitLambda(settingsWidget->shadowCascadeSplitLambda());
    state.shadows().setShadowBias(settingsWidget->shadowBias());
    state.shadows().setShadowSlopeBias(settingsWidget->shadowSlopeBias());
    state.shadows().setShadowFilterRadius(settingsWidget->shadowFilterRadius());
    if (settingsWidget->shadowFilterMode() == "PCSS") {
      state.shadows().setShadowFilterMode(engine::raster::Rasterizer::ShadowFilterMode::PCSS);
    }
    return state;
  }

  engine::graph::RenderPlan rasterPlan() const {
    const QSize resolution = settingsWidget->resolution();
    const auto intent = rasterIntent();
    engine::graph::RenderGraphCompiler compiler;
    auto plan = compiler.compile(
      {resolution.width(), resolution.height(), settingsWidget->msaaSamples()}, intent);
    const engine::graph::RasterBeautyPassState rasterState = rasterBeautyPassState(
      intent.postProcessAA, !intent.usesGraphImagePostProcessAA(), !intent.enablePreviewShadows);
    rasterState.writeToRasterBeautyPasses(plan);
    rasterState.writeToRasterAOVPasses(plan);
    if (intent.enablePreviewShadows) {
      rasterShadowPassState().writeToRasterShadowPasses(plan);
    }
    return plan;
  }

  bool busy;
  int timer;
  QElapsedTimer time;
};

RenderWindow::RenderWindow(QWidget* parent)
    : QWidget(parent),
      p(std::make_unique<Private>()) {
  p->raytracer = std::make_shared<Raytracer>(nullptr);
  p->wireframe = std::make_shared<engine::wireframe::Wireframe>(nullptr);
  p->rasterGraph = std::make_shared<engine::graph::GraphRenderEngine>(nullptr);

  auto grid = new QGridLayout(this);
  p->settingsWidget = new RenderSettingsWidget(this);
  auto scrollArea = new QScrollArea(this);
  p->renderWidget = new RenderWidget(scrollArea, p->raytracer);
  scrollArea->setWidget(p->renderWidget);

  grid->addWidget(p->settingsWidget, 0, 0);
  grid->addWidget(scrollArea, 0, 1);

  setLayout(grid);

  connect(p->settingsWidget, SIGNAL(renderClicked()), this, SLOT(render()));
  connect(p->settingsWidget, SIGNAL(stopClicked()), this, SLOT(stop()));

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

  p->renderWidget->resize(p->settingsWidget->resolution());
  p->renderWidget->setBufferSize(p->settingsWidget->resolution());

  // Pick the engine and wire it into the widget. Some settings are
  // engine-specific: wireframe ignores sampler / recursion depth /
  // threading knobs; rasterizer uses LOD and keeps its default
  // single-tile path unless a caller explicitly changes its queue.
  std::shared_ptr<render::RenderEngine> engine;
  if (p->settingsWidget->engine() == "Wireframe") {
    p->wireframe->setCamera(p->raytracer->camera());
    p->wireframe->setScene(p->raytracer->scene());
    p->wireframe->setLod(p->settingsWidget->lod());
    engine = p->wireframe;
  } else if (p->settingsWidget->engine() == "Rasterizer") {
    const auto intent = p->rasterIntent();
    p->rasterGraph->setCamera(p->raytracer->camera());
    p->rasterGraph->setScene(p->raytracer->scene());
    p->rasterGraph->setIntent(intent);
    p->rasterGraph->setPlan(p->rasterPlan());
    engine = p->rasterGraph;
  } else {
    auto samplerClass = p->settingsWidget->sampler().toStdString() + "Sampler";
    auto sampler = render::SamplerFactory::self().createShared(samplerClass);
    // 83 is an arbitrary number, but it's a relatively large prime number, so
    // it's unlikely to introduce aliasing patterns
    sampler->setup(p->settingsWidget->samplesPerPixel(), 83);

    auto viewPlaneClass = p->settingsWidget->viewPlane().toStdString();
    auto viewPlane = render::ViewPlaneFactory::self().createShared(viewPlaneClass);
    viewPlane->setSampler(sampler);

    p->raytracer->camera()->setViewPlane(viewPlane);
    p->raytracer->setMaximumRecursionDepth(p->settingsWidget->maxRecursionDepth());
    p->raytracer->setMaximumThreads(p->settingsWidget->renderThreads());
    p->raytracer->setQueueSize(p->settingsWidget->queueSize());

    p->raytracer->setShowProgressIndicators(p->settingsWidget->showProgressIndicators());
    engine = p->raytracer;
  }
  p->renderWidget->setEngine(engine);
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
}

void RenderWindow::setScene(::Scene* scene) {
  auto raytracerScene = scene->toRaytracerScene();

  p->raytracer->setScene(raytracerScene);
  p->wireframe->setScene(raytracerScene);
  p->rasterGraph->setScene(raytracerScene);

  auto camera = scene->activeCamera();
  std::shared_ptr<render::Camera> rtCamera;
  if (camera) {
    rtCamera = camera->toRaytracer();
  } else {
    rtCamera = p->raytracer->camera();
    rtCamera->setPosition(Matrix3d::rotateY(-25_degrees) * Matrix3d::rotateX(-25_degrees) *
                          Vector3d(0, 0, -5));
  }
  p->raytracer->setCamera(rtCamera);
  p->wireframe->setCamera(rtCamera);
  p->rasterGraph->setCamera(rtCamera);
}
