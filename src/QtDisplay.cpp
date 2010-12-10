#include "QtDisplay.h"
#include "Raytracer.h"
#include "Buffer.h"
#include "Camera.h"

#include <QMouseEvent>
#include <QThread>

#include <iostream>
using namespace std;

class RenderThread : public QThread {
public:
  RenderThread(Raytracer* rt, Buffer& b)
    : raytracer(rt), buffer(b) {}

  virtual void run() {
    raytracer->render(buffer);
  }
  
  void cancel() {
    raytracer->cancel();
  }

  Raytracer* raytracer;
  Buffer& buffer;
};

QtDisplay::QtDisplay(Raytracer* raytracer)
  : QWidget(), m_raytracer(raytracer), m_xAngle(0), m_yAngle(0), m_distance(1), m_buffer(0), m_renderThread(0)
{
  m_buffer = new Buffer(width(), height());
  resize(400, 300);
  render();
}

void QtDisplay::stop() {
  if (m_renderThread && m_renderThread->isRunning()) {
    m_renderThread->cancel();
    m_renderThread->wait();
  }
  
  if (m_renderThread) {
    delete m_renderThread;
    m_renderThread = 0;
  }
  killTimer(m_timer);
}

void QtDisplay::render() {
  stop();
  m_raytracer->camera()->setPosition(Matrix3d::rotateX(m_xAngle) * Matrix3d::rotateY(m_yAngle) * Vector3d(0, 0, -m_distance));
  m_renderThread = new RenderThread(m_raytracer, *m_buffer);
  m_renderThread->start();
  connect(m_renderThread, SIGNAL(finished()), this, SLOT(update()));
  m_timer = startTimer(50);
}

void QtDisplay::resizeEvent(QResizeEvent*) {
  stop();
  if (m_buffer)
    delete m_buffer;
  m_buffer = new Buffer(width(), height());
  render();
}

void QtDisplay::timerEvent(QTimerEvent*) {
  update();
}

void QtDisplay::paintEvent(QPaintEvent*) {
  if (m_renderThread && !m_renderThread->isRunning())
    stop();
  
  QPainter painter(this);
  QImage image(width(), height(), QImage::Format_RGB32);
  
  for (int i = 0; i != width(); ++i) {
    for (int j = 0; j != height(); ++j) {
      image.setPixel(i, j, (*m_buffer)[j][i].rgb());
    }
  }
  
  painter.drawImage(QPoint(0, 0), image);
}

void QtDisplay::mousePressEvent(QMouseEvent* event) {
  m_dragPosition = event->pos();
}

void QtDisplay::mouseMoveEvent(QMouseEvent* event) {
  QPoint delta = event->pos() - m_dragPosition;
  
  m_xAngle -= delta.y() * 0.01;
  if (m_xAngle < -1)
    m_xAngle = -1;
  if (m_xAngle > 1)
    m_xAngle = 1;
  m_yAngle += delta.x() * 0.01;
  
  render();
  
  m_dragPosition = event->pos();
}

void QtDisplay::wheelEvent(QWheelEvent* event) {
  if (event->delta() < 0)
    m_distance /= 1.2;
  else
    m_distance *= 1.2;
  render();
}
