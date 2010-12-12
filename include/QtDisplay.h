#ifndef QT_DISPLAY_H
#define QT_DISPLAY_H

#include <QWidget>
#include <QPainter>

#include "Buffer.h"

class Raytracer;
class RenderThread;

class QtDisplay : public QWidget {
public:
  QtDisplay(Raytracer* raytracer);
  
  virtual void paintEvent(QPaintEvent*);
  virtual void mouseMoveEvent(QMouseEvent* event);
  virtual void mousePressEvent(QMouseEvent* event);
  virtual void wheelEvent(QWheelEvent* event);
  virtual void resizeEvent(QResizeEvent* event);
  virtual void timerEvent(QTimerEvent *event);
  
  void stop();
  void render();
  
protected:
  Raytracer* m_raytracer;

private:
  double m_xAngle, m_yAngle, m_distance;
  QPoint m_dragPosition;
  Buffer* m_buffer;
  RenderThread* m_renderThread;
  int m_timer;
};

#endif
