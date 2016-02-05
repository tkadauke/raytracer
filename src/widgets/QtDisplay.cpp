#include "widgets/QtDisplay.h"
#include "raytracer/Raytracer.h"
#include "core/Buffer.h"
#include "raytracer/cameras/Camera.h"

#include <QMouseEvent>
#include <QThread>
#include <QPainter>

using namespace std;
using namespace raytracer;

namespace {
  class RenderThread : public QThread {
  public:
    RenderThread(Raytracer* rt, Buffer<unsigned int>& b)
      : raytracer(rt), buffer(b) {}

    virtual void run() {
      raytracer->render(buffer);
    }
  
    void cancel() {
      raytracer->cancel();
    }

    Raytracer* raytracer;
    Buffer<unsigned int>& buffer;
  };
}

struct QtDisplay::Private {
  Private()
    : renderThread(0), xAngle(0), yAngle(0), distance(1), buffer(0) {}
  
  RenderThread* renderThread;
  
  double xAngle, yAngle, distance;
  QPoint dragPosition;
  Buffer<unsigned int>* buffer;
  int timer;
};

QtDisplay::QtDisplay(QWidget* parent, Raytracer* raytracer)
  : QWidget(parent), m_raytracer(raytracer), p(new Private)
{
  p->buffer = new Buffer<unsigned int>(width(), height());
  resize(400, 300);
}

QtDisplay::~QtDisplay() {
  delete p;
}

void QtDisplay::stop() {
  if (p->renderThread && p->renderThread->isRunning()) {
    p->renderThread->cancel();
    p->renderThread->wait();
  }

  if (p->renderThread) {
    delete p->renderThread;
    p->renderThread = 0;
  }
  update();
}

void QtDisplay::render() {
  stop();
  m_raytracer->uncancel();
  m_raytracer->camera()->setPosition(Matrix3d::rotateY(p->yAngle) * Matrix3d::rotateX(p->xAngle) * Vector3d(0, 0, -p->distance));
  p->renderThread = new RenderThread(m_raytracer, *p->buffer);
  p->renderThread->start();
  connect(p->renderThread, SIGNAL(finished()), this, SLOT(update()));
  p->timer = startTimer(50);
}

void QtDisplay::resizeEvent(QResizeEvent*) {
  stop();
  if (p->buffer)
    delete p->buffer;
  p->buffer = new Buffer<unsigned int>(width(), height());
  render();
}

void QtDisplay::timerEvent(QTimerEvent*) {
  update();
}

void QtDisplay::paintEvent(QPaintEvent*) {
  if (p->renderThread && !p->renderThread->isRunning())
    stop();
  
  QPainter painter(this);
  QImage image(width(), height(), QImage::Format_RGB32);
  
  for (int i = 0; i != width(); ++i) {
    for (int j = 0; j != height(); ++j) {
      image.setPixel(i, j, (*p->buffer)[j][i]);
    }
  }
  
  painter.drawImage(QPoint(0, 0), image);
}

void QtDisplay::mousePressEvent(QMouseEvent* event) {
  p->dragPosition = event->pos();
}

void QtDisplay::mouseMoveEvent(QMouseEvent* event) {
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
  if (event->delta() < 0)
    p->distance /= 1.05;
  else
    p->distance *= 1.05;
  render();
}

void QtDisplay::setDistance(double distance) {
  p->distance = distance;
}

#include "QtDisplay.moc"
