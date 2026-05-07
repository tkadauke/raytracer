#pragma once

#include "widgets/QtDisplay.h"

class Scene;
class Element;

namespace render {
  class Camera;
  class WireframeEngine;
}

namespace engine::raytracer {
  class Raytracer;
}

class Display : public QtDisplay {
  Q_OBJECT;

  virtual void mousePressEvent(QMouseEvent* event);

public:
  Display(QWidget* parent);
  ~Display();

  void setScene(Scene* scene);

  /// Engine kinds supported by the modeling preview. The render
  /// dialog has its own selector — this one only affects the
  /// always-on preview pane.
  enum class EngineKind { Raytracer, Wireframe };

public slots:
  /// Swap the live preview engine. The new engine inherits the
  /// previous engine's scene and camera, so the user sees the same
  /// view rendered through the new engine.
  void setEngineKind(EngineKind kind);

private:
  std::shared_ptr<engine::raytracer::Raytracer> m_raytracerEngine;
  std::shared_ptr<render::WireframeEngine> m_wireframeEngine;
};
