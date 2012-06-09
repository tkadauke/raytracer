#ifndef QT_DISPLAY_H
#define QT_DISPLAY_H

#include <QWidget>
#include <QPainter>

#include "Buffer.h"

class Raytracer;

class QtDisplay : public QWidget {
  Q_OBJECT
public:
  QtDisplay(Raytracer* raytracer);
  ~QtDisplay();
  
  virtual void paintEvent(QPaintEvent*);
  virtual void mouseMoveEvent(QMouseEvent* event);
  virtual void mousePressEvent(QMouseEvent* event);
  virtual void wheelEvent(QWheelEvent* event);
  virtual void resizeEvent(QResizeEvent* event);
  virtual void timerEvent(QTimerEvent *event);
  
  void render();
  
  void setDistance(double distance);

public slots:
  void stop();
  
protected:
  Raytracer* m_raytracer;

private:
  class Private;
  Private* p;
};

#endif
