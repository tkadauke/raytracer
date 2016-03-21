#include "widgets/world/RenderWindow.h"
#include "widgets/world/RenderSettingsWidget.h"

#include "widgets/RenderWidget.h"

#include "raytracer/Raytracer.h"
#include "raytracer/lights/PointLight.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/cameras/Camera.h"

#include "raytracer/samplers/SamplerFactory.h"
#include "raytracer/viewplanes/ViewPlaneFactory.h"

#include "world/objects/Scene.h"
#include "world/objects/Camera.h"

#include <QGridLayout>
#include <QScrollArea>
#include <QTime>

using namespace raytracer;

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
  
  std::shared_ptr<raytracer::Raytracer> raytracer;
  
  bool busy;
  int timer;
  QTime time;
};

RenderWindow::RenderWindow(QWidget* parent)
  : QWidget(parent),
    p(std::make_unique<Private>())
{
  p->raytracer = std::make_shared<Raytracer>(nullptr);
  
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
  
  auto samplerClass = p->settingsWidget->sampler().toStdString() + "Sampler";
  auto sampler = SamplerFactory::self().createShared(samplerClass);
  // 83 is an arbitrary number, but it's a relatively large prime number, so
  // it's unlikely to introduce aliasing patterns
  sampler->setup(p->settingsWidget->samplesPerPixel(), 83);
  
  auto viewPlaneClass = p->settingsWidget->viewPlane().toStdString();
  auto viewPlane = ViewPlaneFactory::self().createShared(viewPlaneClass);
  p->raytracer->camera()->setViewPlane(viewPlane);
  p->raytracer->camera()->viewPlane()->setSampler(sampler);
  p->raytracer->setMaximumRecursionDepth(p->settingsWidget->maxRecursionDepth());
  p->raytracer->setMaximumThreads(p->settingsWidget->renderThreads());
  p->raytracer->setQueueSize(p->settingsWidget->queueSize());
  
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
  
  auto camera = scene->activeCamera();
  if (camera) {
    p->raytracer->setCamera(camera->toRaytracer());
  } else {
    p->raytracer->camera()->setPosition(
      Matrix3d::rotateY(-25 * Degreed) *
      Matrix3d::rotateX(-25 * Degreed) *
      Vector3d(0, 0, -5)
    );
  }
}

#include "RenderWindow.moc"
