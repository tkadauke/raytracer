#pragma once
#include <memory>

#include "widgets/RenderWidget.h"

namespace render {
  class RenderEngine;
}

class QtDisplay : public RenderWidget {
  Q_OBJECT
public:
  explicit QtDisplay(QWidget* parent, std::shared_ptr<render::RenderEngine> engine);
  ~QtDisplay();
  
  void setInteractive(bool interactive);
  bool interactive() const;

  /// Controls whether a camera interaction cancels an unfinished
  /// preview frame. Raytraced previews use this for immediate live
  /// feedback; double-buffered raster/wireframe previews can defer
  /// until the current frame completes.
  void setCancelRenderOnInteraction(bool cancel);
  bool cancelRenderOnInteraction() const;
  
  virtual void mouseMoveEvent(QMouseEvent* event);
  virtual void mousePressEvent(QMouseEvent* event);
  virtual void wheelEvent(QWheelEvent* event);
  virtual void resizeEvent(QResizeEvent* event);

  virtual void render();
  
  void setDistance(double distance);

private:
  void renderAfterCurrentFrameIfRequested();

  struct Private;
  std::unique_ptr<Private> p;
};
