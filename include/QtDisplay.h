#ifndef QT_DISPLAY_H
#define QT_DISPLAY_H

#include <QWidget>
#include <QPainter>

class Raytracer;

class QtDisplay : public QWidget {
public:
  QtDisplay(Raytracer* raytracer);
  
  virtual void paintEvent(QPaintEvent*);
  virtual void mouseMoveEvent(QMouseEvent* event);
  virtual void mousePressEvent(QMouseEvent* event);
  
private:
  QWidget* m_window;
  Raytracer* m_raytracer;
  double m_xAngle, m_yAngle;
  QPoint m_dragPosition;
};

#endif
