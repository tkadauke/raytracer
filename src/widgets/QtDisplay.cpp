#include "widgets/QtDisplay.h"
#include "render/RenderEngine.h"
#include "render/cameras/Camera.h"

#include <algorithm>
#include <cmath>
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
        distance(1),
        target(Vector3d::zero) {
  }

  bool interactive;
  bool cancelRenderOnInteraction;
  bool renderAfterCurrentFrame;
  double xAngle, yAngle, distance;
  Vector3d target;
  QPoint dragPosition;
};

QtDisplay::QtDisplay(QWidget* parent, std::shared_ptr<render::RenderEngine> engine)
    : RenderWidget(parent, std::move(engine)),
      p(std::make_unique<Private>()) {
  resize(400, 300);
  setBufferSize(size());
  connect(this, &RenderWidget::finished, this, [this] {
    QTimer::singleShot(0, this, [this] { renderAfterCurrentFrameIfRequested(); });
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
    if (p->cancelRenderOnInteraction) {
      cancelRender();
      if (isRendering()) {
        p->renderAfterCurrentFrame = true;
        return;
      }
    } else {
      p->renderAfterCurrentFrame = true;
      return;
    }
  }

  p->renderAfterCurrentFrame = false;
  if (interactive()) {
    m_engine->camera()->setTarget(p->target);
    m_engine->camera()->setPosition(p->target +
                                    Matrix3d::rotateY(Angled::fromRadians(p->yAngle)) *
                                      Matrix3d::rotateX(Angled::fromRadians(p->xAngle)) *
                                      Vector3d(0, 0, -p->distance));
  }

  RenderWidget::render();
}

void QtDisplay::setDistance(double distance) {
  p->distance = distance;
}

void QtDisplay::setInteractiveCameraPose(const Vector3d& position, const Vector3d& target) {
  const auto offset = position - target;
  p->distance = std::max(offset.length(), 0.000001);
  p->target = target;
  p->xAngle = std::asin(std::clamp(offset.y() / p->distance, -1.0, 1.0));
  p->yAngle = std::atan2(-offset.x(), -offset.z());
}

void QtDisplay::renderAfterCurrentFrameIfRequested() {
  if (!p->renderAfterCurrentFrame)
    return;

  render();
}
