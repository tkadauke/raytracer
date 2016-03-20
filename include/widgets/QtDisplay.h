#pragma once

#include "widgets/RenderWidget.h"

namespace raytracer {
  class Raytracer;
}

class QtDisplay : public RenderWidget {
  Q_OBJECT;
public:
  explicit QtDisplay(QWidget* parent, std::shared_ptr<raytracer::Raytracer> raytracer);
  ~QtDisplay();
  
  void setInteractive(bool interactive);
  bool interactive() const;
  
  virtual void mouseMoveEvent(QMouseEvent* event);
  virtual void mousePressEvent(QMouseEvent* event);
  virtual void wheelEvent(QWheelEvent* event);
  virtual void resizeEvent(QResizeEvent* event);

  virtual void render();
  
  void setDistance(double distance);

private:
  struct Private;
  std::unique_ptr<Private> p;
};
