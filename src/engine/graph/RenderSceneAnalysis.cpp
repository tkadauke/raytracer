#include "engine/graph/RenderSceneAnalysis.h"

#include <utility>

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

  void RenderSceneAnalysis::recordPortalReceiverSurface(std::string surfaceId,
                                                        std::string surfaceName) {
    m_portalReceiverSurfaces.push_back(
      SceneSurfaceMarker{std::move(surfaceId), std::move(surfaceName)});
  }

  void RenderSceneAnalysis::recordPlanarMirrorSurface(std::string surfaceId,
                                                      std::string surfaceName) {
    m_planarMirrorSurfaces.push_back(
      SceneSurfaceMarker{std::move(surfaceId), std::move(surfaceName)});
  }

  void RenderSceneAnalysis::recordRenderTextureReceiver(std::string subviewName) {
    if (!subviewName.empty()) {
      m_renderTextureSubviewReceivers.insert(std::move(subviewName));
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

  std::size_t RenderSceneAnalysis::portalReceiverSurfaceCount() const {
    return m_portalReceiverSurfaces.size();
  }

  std::size_t RenderSceneAnalysis::planarMirrorSurfaceCount() const {
    return m_planarMirrorSurfaces.size();
  }

  const std::vector<RenderSceneAnalysis::SceneSurfaceMarker>&
  RenderSceneAnalysis::portalReceiverSurfaces() const {
    return m_portalReceiverSurfaces;
  }

  const std::vector<RenderSceneAnalysis::SceneSurfaceMarker>&
  RenderSceneAnalysis::planarMirrorSurfaces() const {
    return m_planarMirrorSurfaces;
  }

  const std::set<std::string>& RenderSceneAnalysis::renderTextureSubviewReceivers() const {
    return m_renderTextureSubviewReceivers;
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
