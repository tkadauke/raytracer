#include "widgets/QtDisplay.h"
#include "render/RenderEngine.h"
#include "render/cameras/Camera.h"

#include <QMouseEvent>
#include <QTimer>

using namespace std;
using namespace render;

struct QtDisplay::Private {
  Private()
    : interactive(true),
      cancelRenderOnInteraction(true),
      renderAfterCurrentFrame(false),
      xAngle(0),
      yAngle(0),
      distance(1)
  {
  }
  
  bool interactive;
  bool cancelRenderOnInteraction;
  bool renderAfterCurrentFrame;
  double xAngle, yAngle, distance;
  QPoint dragPosition;
};

QtDisplay::QtDisplay(QWidget* parent, std::shared_ptr<render::RenderEngine> engine)
  : RenderWidget(parent, std::move(engine)),
    p(std::make_unique<Private>())
{
  setBufferSize(size());
  resize(400, 300);
  connect(this, &RenderWidget::finished, this, [this] {
    QTimer::singleShot(0, this, [this] {
      renderAfterCurrentFrameIfRequested();
    });
  });
}

QtDisplay::~QtDisplay() {
}

void QtDisplay::setInteractive(bool interactive) {
  p->interactive = interactive;
}

bool QtDisplay::interactive() const {
  return p->interactive;
}

void QtDisplay::setCancelRenderOnInteraction(bool cancel) {
  p->cancelRenderOnInteraction = cancel;
}

bool QtDisplay::cancelRenderOnInteraction() const {
  return p->cancelRenderOnInteraction;
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
  if (isRendering()) {
    p->renderAfterCurrentFrame = true;
    if (p->cancelRenderOnInteraction) {
      cancelRender();
    }
    return;
  }

  p->renderAfterCurrentFrame = false;
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

void QtDisplay::renderAfterCurrentFrameIfRequested() {
  if (!p->renderAfterCurrentFrame)
    return;

  render();
}
