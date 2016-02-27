#include "widgets/world/RenderWindow.h"
#include "widgets/world/RenderSettingsWidget.h"

#include "widgets/RenderWidget.h"

#include "raytracer/Raytracer.h"
#include "raytracer/Light.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/cameras/Camera.h"

#include "raytracer/samplers/RegularSampler.h"

#include "world/objects/Scene.h"

#include <QGridLayout>
#include <QScrollArea>
#include <QTime>

using namespace raytracer;

struct RenderWindow::Private {
  Private() : busy(false), timer(0) {}
  
  RenderWidget* renderWidget;
  RenderSettingsWidget* settingsWidget;
  
  std::shared_ptr<raytracer::Raytracer> raytracer;
  
  bool busy;
  int timer;
  QTime time;
};

RenderWindow::RenderWindow(QWidget* parent)
  : QWidget(parent), p(std::make_unique<Private>())
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
  p->raytracer->camera()->viewPlane()->setSampler(std::make_shared<raytracer::RegularSampler>(p->settingsWidget->samplesPerPixel()));
  p->raytracer->setMaximumRecursionDepth(p->settingsWidget->maxRecursionDepth());
  
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
  
  auto light1 = new raytracer::Light(Vector3d(-30, -30, -10), Colord(0.4, 0.4, 0.4));
  raytracerScene->addLight(light1);
  
  p->raytracer->setScene(raytracerScene);
  
  p->raytracer->camera()->setPosition(
    Matrix3d::rotateY(Angled::fromDegrees(-25)) *
    Matrix3d::rotateX(Angled::fromDegrees(-25)) *
    Vector3d(0, 0, -5)
  );
}

#include "RenderWindow.moc"
