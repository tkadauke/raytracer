#pragma once

#include "widgets/QtDisplay.h"

class Scene;
class Element;
class QResizeEvent;

namespace render {
  class Camera;
  class Tonemap;
}

namespace engine::raytracer {
  class Raytracer;
}

namespace engine::graph {
  class GraphRenderEngine;
  class RenderPlan;
  enum class RenderPostProcessAA;
  struct RenderIntent;
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

  /// Select image-space anti-aliasing for the live Rasterizer preview graph.
  void setRasterizerPreviewPostProcessAA(engine::graph::RenderPostProcessAA aa);
  engine::graph::RenderPostProcessAA rasterizerPreviewPostProcessAA() const;

  /// Toggle a graph-level wireframe overlay over the shaded live preview.
  void setWireframeOverlayEnabled(bool enabled);
  bool wireframeOverlayEnabled() const;

  /// Installs the latest graph intent that produced the preview plan.
  void setRenderGraphIntent(const engine::graph::RenderIntent& intent);

  /// Installs the effective graph plan that the live preview should render.
  void setRenderGraphPlan(const engine::graph::RenderPlan& plan);

  /// Sets the tonemap used by the preview graph's tonemap node.
  void setPreviewTonemap(std::shared_ptr<render::Tonemap> tonemap);

private:
  void applyPreviewPolicy(EngineKind kind);

  std::shared_ptr<engine::graph::GraphRenderEngine> m_graphEngine;
  std::shared_ptr<engine::raytracer::Raytracer> m_raytracerEngine;
  EngineKind m_engineKind{EngineKind::Raytracer};
  bool m_rasterizerPreviewShadowsEnabled{false};
  engine::graph::RenderPostProcessAA m_rasterizerPreviewPostProcessAA;
  bool m_wireframeOverlayEnabled{false};
};
