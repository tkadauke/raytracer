#pragma once

#include "widgets/QtDisplay.h"

class Scene;
class Element;
class QResizeEvent;

namespace render {
  class Camera;
}

namespace engine::wireframe {
  class Wireframe;
}

namespace engine::raster {
  class Rasterizer;
}

namespace engine::raytracer {
  class Raytracer;
}

class RenderDisplay : public QtDisplay {
  Q_OBJECT

signals:
  void renderGraphInputsChanged();

protected:
  void resizeEvent(QResizeEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

public:
  RenderDisplay(QWidget* parent);
  ~RenderDisplay();

  void setScene(Scene* scene);

  /// Engine kinds supported by the modeling preview. The render
  /// dialog has its own selector — this one only affects the
  /// always-on preview pane.
  enum class EngineKind { Raytracer, Wireframe, Rasterizer };

public slots:
  /// Swap the live preview engine. The new engine inherits the
  /// previous engine's scene and camera, so the user sees the same
  /// view rendered through the new engine.
  void setEngineKind(EngineKind kind);
  EngineKind engineKind() const;

  /// Toggle directional shadow maps for the live Rasterizer preview.
  /// Final renders keep using RenderWindow's explicit render settings.
  void setRasterizerPreviewShadowsEnabled(bool enabled);
  bool rasterizerPreviewShadowsEnabled() const;

private:
  void applyPreviewPolicy(EngineKind kind);
  void applyRasterizerPreviewPolicy();

  std::shared_ptr<engine::raytracer::Raytracer> m_raytracerEngine;
  std::shared_ptr<engine::wireframe::Wireframe> m_wireframeEngine;
  std::shared_ptr<engine::raster::Rasterizer> m_rasterizerEngine;
  EngineKind m_engineKind{EngineKind::Raytracer};
  bool m_rasterizerPreviewShadowsEnabled{false};
};
