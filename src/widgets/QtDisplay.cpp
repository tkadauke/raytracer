#include "widgets/QtDisplay.h"
#include "raytracer/RenderEngine.h"
#include "render/cameras/Camera.h"

#include <QMouseEvent>

using namespace std;
using namespace raytracer;

struct QtDisplay::Private {
  Private()
    : interactive(true),
      xAngle(0),
      yAngle(0),
      distance(1)
  {
  }
  
  bool interactive;
  double xAngle, yAngle, distance;
  QPoint dragPosition;
};

QtDisplay::QtDisplay(QWidget* parent, std::shared_ptr<RenderEngine> engine)
  : RenderWidget(parent, std::move(engine)),
    p(std::make_unique<Private>())
{
  setBufferSize(size());
  resize(400, 300);
}

QtDisplay::~QtDisplay() {
}

void QtDisplay::setInteractive(bool interactive) {
  p->interactive = interactive;
}

bool QtDisplay::interactive() const {
  return p->interactive;
}

void QtDisplay::resizeEvent(QResizeEvent*) {
  stop();
  setBufferSize(size());
  render();
}

void QtDisplay::mousePressEvent(QMouseEvent* event) {
  if (!interactive()) {
    return;
  }
  p->dragPosition = event->pos();
}

void QtDisplay::mouseMoveEvent(QMouseEvent* event) {
  if (!interactive()) {
    return;
  }
  QPoint delta = event->pos() - p->dragPosition;
  
  p->xAngle -= delta.y() * 0.01;
  if (p->xAngle < -1)
    p->xAngle = -1;
  if (p->xAngle > 1)
    p->xAngle = 1;
  p->yAngle += delta.x() * 0.01;
  
  render();
  
  p->dragPosition = event->pos();
}

void QtDisplay::wheelEvent(QWheelEvent* event) {
  if (!interactive()) {
    return;
  }
  if (event->angleDelta().y() < 0)
    p->distance /= 1.05;
  else
    p->distance *= 1.05;
  render();
}

void QtDisplay::render() {
  if (interactive()) {
    m_engine->camera()->setPosition(
      Matrix3d::rotateY(Angled::fromRadians(p->yAngle)) *
      Matrix3d::rotateX(Angled::fromRadians(p->xAngle)) *
      Vector3d(0, 0, -p->distance)
    );
  }

  RenderWidget::render();
}

void QtDisplay::setDistance(double distance) {
  p->distance = distance;
}

