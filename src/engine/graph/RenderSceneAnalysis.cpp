#include "engine/graph/RenderSceneAnalysis.h"

namespace engine::graph {
  RenderSceneAnalysis::RenderSceneAnalysis() = default;

  RenderSceneAnalysis RenderSceneAnalysis::unknownScene() {
    RenderSceneAnalysis analysis;
    analysis.m_visibleSurfaceCount.reset();
    analysis.m_visibleLightCount.reset();
    return analysis;
  }

  void RenderSceneAnalysis::recordVisibleSurface() {
    if (m_visibleSurfaceCount) {
      ++*m_visibleSurfaceCount;
    }
  }

  void RenderSceneAnalysis::recordVisibleLight() {
    if (m_visibleLightCount) {
      ++*m_visibleLightCount;
    }
  }

  bool RenderSceneAnalysis::hasKnownVisibleSurfaceCount() const {
    return m_visibleSurfaceCount.has_value();
  }

  bool RenderSceneAnalysis::hasKnownVisibleLightCount() const {
    return m_visibleLightCount.has_value();
  }

  std::size_t RenderSceneAnalysis::visibleSurfaceCount() const {
    return m_visibleSurfaceCount.value_or(0);
  }

  std::size_t RenderSceneAnalysis::visibleLightCount() const {
    return m_visibleLightCount.value_or(0);
  }

  bool RenderSceneAnalysis::hasVisibleSurfaces() const {
    return !m_visibleSurfaceCount || *m_visibleSurfaceCount > 0;
  }

  bool RenderSceneAnalysis::hasVisibleLights() const {
    return !m_visibleLightCount || *m_visibleLightCount > 0;
  }

  bool RenderSceneAnalysis::shouldCompileRasterPreviewShadows(RenderExecutorKind executor,
                                                              const RenderIntent& intent) const {
    return intent.enablePreviewShadows && executor == RenderExecutorKind::Rasterizer &&
           m_rasterShadowMapsSupported && hasVisibleSurfaces() && hasVisibleLights();
  }
}
