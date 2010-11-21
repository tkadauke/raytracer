#include "QtDisplay.h"
#include "Raytracer.h"
#include "Buffer.h"
#include "Camera.h"

#include <QMouseEvent>

#include <iostream>
using namespace std;

QtDisplay::QtDisplay(Raytracer* raytracer)
  : QWidget(), m_raytracer(raytracer), m_xAngle(0), m_yAngle(0)
{
  resize(400, 300);
}

void QtDisplay::paintEvent(QPaintEvent*)
{
  QPainter painter(this);
  Buffer buffer(width(), height());
  Camera camera(Matrix3d::rotateY(m_yAngle) * Matrix3d::rotateX(m_xAngle) * Vector3d(0, 0, -1), Vector3d::null);
  m_raytracer->render(camera, buffer);
  
  QImage image(width(), height(), QImage::Format_RGB32);
  
  for (int i = 0; i != width(); ++i) {
    for (int j = 0; j != height(); ++j) {
      image.setPixel(i, j, buffer[j][i].rgb());
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
  
  update();
  
  m_dragPosition = event->pos();
}
