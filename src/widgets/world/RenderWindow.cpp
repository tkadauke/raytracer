#include "widgets/world/RenderWindow.h"
#include "widgets/world/RenderSettingsWidget.h"

#include "widgets/RenderWidget.h"

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
      timer(0)
  {
  }

  RenderWidget* renderWidget;
  RenderSettingsWidget* settingsWidget;

  // Per-engine instances kept around so swapping is cheap. Only one
  // is wired into renderWidget at a time; the other holds onto its
  // scene + camera ready to take over on the next "Render" click.
  std::shared_ptr<engine::raytracer::Raytracer> raytracer;
  std::shared_ptr<engine::wireframe::Wireframe> wireframe;
  std::shared_ptr<engine::raster::Rasterizer> rasterizer;

  bool busy;
  int timer;
  QElapsedTimer time;
};

RenderWindow::RenderWindow(QWidget* parent)
  : QWidget(parent),
    p(std::make_unique<Private>())
{
  p->raytracer = std::make_shared<Raytracer>(nullptr);
  p->wireframe = std::make_shared<engine::wireframe::Wireframe>(nullptr);
  p->rasterizer = std::make_shared<engine::raster::Rasterizer>(nullptr);

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

  // Pick the engine and wire it into the widget. Most settings only
  // apply to the raytracer; wireframe ignores sampler / recursion
  // depth / threading knobs.
  std::shared_ptr<render::RenderEngine> engine;
  if (p->settingsWidget->engine() == "Wireframe") {
    p->wireframe->setCamera(p->raytracer->camera());
    p->wireframe->setScene(p->raytracer->scene());
    p->wireframe->setLod(p->settingsWidget->lod());
    engine = p->wireframe;
  } else if (p->settingsWidget->engine() == "Rasterizer") {
    p->rasterizer->setCamera(p->raytracer->camera());
    p->rasterizer->setScene(p->raytracer->scene());
    p->rasterizer->setLod(p->settingsWidget->lod());
    engine = p->rasterizer;
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
  p->rasterizer->setScene(raytracerScene);

  auto camera = scene->activeCamera();
  std::shared_ptr<render::Camera> rtCamera;
  if (camera) {
    rtCamera = camera->toRaytracer();
  } else {
    rtCamera = p->raytracer->camera();
    rtCamera->setPosition(
      Matrix3d::rotateY(-25_degrees) *
      Matrix3d::rotateX(-25_degrees) *
      Vector3d(0, 0, -5)
    );
  }
  p->raytracer->setCamera(rtCamera);
  p->wireframe->setCamera(rtCamera);
  p->rasterizer->setCamera(rtCamera);
}

