#pragma once

#include "widgets/QtDisplay.h"

#include "engine/raster/RasterBackend.h"

#include <cstdint>

#include <QString>

class Scene;
class Element;
class QResizeEvent;
struct StepPlaybackStyle;

namespace render {
  class Camera;
  class Tonemap;
}

namespace engine::raytracer {
  class Raytracer;
}

namespace engine::graph {
  class GraphRenderEngine;
  class RenderGraphExecutionObserver;
  class RenderGraphExecutionTrace;
  class RenderPlan;
  enum class RenderPostProcessAA;
  enum class RenderViewMode;
  struct RenderIntent;
}

class RenderDisplay : public QtDisplay {
  Q_OBJECT

signals:
  void renderGraphInputsChanged();
  void renderGraphExecutionStarted();
  void renderGraphPassStarted(const QString& passId);
  void renderGraphPassFinished(const QString& passId);
  void renderGraphPassFailed(const QString& passId, const QString& message);

protected:
  void resizeEvent(QResizeEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

public:
  RenderDisplay(QWidget* parent);
  ~RenderDisplay();

  enum class CameraPolicy { ResetToSceneCamera, PreserveCurrent };

  void setScene(Scene* scene);
  void setScene(Scene* scene, const StepPlaybackStyle& playbackStyle,
                CameraPolicy cameraPolicy = CameraPolicy::ResetToSceneCamera);
  void notifyRenderGraphExecutionStarted(std::uint64_t generation);
  void notifyRenderGraphPassStarted(const QString& passId, std::uint64_t generation);
  void notifyRenderGraphPassFinished(const QString& passId, std::uint64_t generation);
  void notifyRenderGraphPassFailed(const QString& passId, const QString& message,
                                   std::uint64_t generation);
  std::shared_ptr<const engine::graph::RenderGraphExecutionTrace>
  lastRenderGraphExecutionTrace() const;
  std::shared_ptr<const engine::graph::RenderGraphExecutionTrace>
  lastRenderGraphExecutionTraceForPlan(const engine::graph::RenderPlan& plan) const;

  /// Engine kinds supported by the modeling preview. The render
  /// dialog has its own selector — this one only affects the
  /// always-on preview pane.
  enum class EngineKind {
    Raytracer,
    ScalarPathTracer,
    PathTracer,
    Wavefront,
    Wireframe,
    Rasterizer
  };

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

  /// Selects the raster backend used when preview overrides compile a
  /// Rasterizer graph. Scene render settings stay authoritative while
  /// "Use Scene Render Settings" is enabled.
  void setRasterizerPreviewBackend(engine::raster::RasterBackend backend);
  engine::raster::RasterBackend rasterizerPreviewBackend() const;

  /// Select image-space anti-aliasing for the live preview graph.
  void setPreviewPostProcessAA(engine::graph::RenderPostProcessAA aa);
  engine::graph::RenderPostProcessAA previewPostProcessAA() const;

  /// Select the structural graph view used by the live preview.
  void setPreviewViewMode(engine::graph::RenderViewMode viewMode);
  engine::graph::RenderViewMode previewViewMode() const;

  /// Toggle a graph-level wireframe overlay over the shaded live preview.
  void setWireframeOverlayEnabled(bool enabled);
  bool wireframeOverlayEnabled() const;

  /// Installs the latest graph intent that produced the preview plan.
  void setRenderGraphIntent(const engine::graph::RenderIntent& intent);

  /// Installs the effective graph plan that the live preview should render.
  void setRenderGraphPlan(const engine::graph::RenderPlan& plan);

  /// Enables or suspends live preview rendering while graph overrides are valid.
  void setRenderGraphPreviewEnabled(bool enabled);
  bool renderGraphPreviewEnabled() const;

  /// Sets the tonemap used by the preview graph's tonemap node.
  void setPreviewTonemap(std::shared_ptr<render::Tonemap> tonemap);

  void render() override;

private:
  void applyPreviewPolicy(EngineKind kind);
  void bindSceneCameras(const Scene& scene);

  std::shared_ptr<engine::graph::GraphRenderEngine> m_graphEngine;
  std::shared_ptr<engine::graph::RenderGraphExecutionObserver> m_graphExecutionObserver;
  std::shared_ptr<engine::raytracer::Raytracer> m_raytracerEngine;
  EngineKind m_engineKind{EngineKind::Raytracer};
  bool m_rasterizerPreviewShadowsEnabled{false};
  engine::raster::RasterBackend m_rasterizerPreviewBackend{engine::raster::RasterBackend::cpu()};
  engine::graph::RenderPostProcessAA m_previewPostProcessAA;
  engine::graph::RenderViewMode m_previewViewMode;
  bool m_wireframeOverlayEnabled{false};
  bool m_renderGraphPreviewEnabled{true};
  bool m_waitingForGraphExecutionStart{false};
  std::uint64_t m_graphExecutionGeneration{0};
};
