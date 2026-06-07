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
                                                        std::string surfaceName,
                                                        Matrix4d receiverTransform,
                                                        Matrix4d sourceTransform,
                                                        bool receiverVisibleInPrimaryView) {
    SceneSurfaceMarker marker;
    marker.surfaceId = std::move(surfaceId);
    marker.surfaceName = std::move(surfaceName);
    marker.receiverTransform = receiverTransform;
    marker.sourceTransform = sourceTransform;
    marker.planePoint = receiverTransform.transformPoint(Vector3d::null);
    marker.planeNormal =
      receiverTransform.transformDirection(Vector3d(0.0, -1.0, 0.0)).normalizedOrZero(1e-12);
    marker.receiverVisibleInPrimaryView = receiverVisibleInPrimaryView;
    m_portalReceiverSurfaces.push_back(std::move(marker));
  }

  void RenderSceneAnalysis::recordPlanarMirrorSurface(std::string surfaceId,
                                                      std::string surfaceName, Vector3d planePoint,
                                                      Vector3d planeNormal,
                                                      bool receiverVisibleInPrimaryView) {
    SceneSurfaceMarker marker;
    marker.surfaceId = std::move(surfaceId);
    marker.surfaceName = std::move(surfaceName);
    marker.planePoint = planePoint;
    marker.planeNormal = planeNormal.normalizedOrZero(1e-12);
    marker.receiverVisibleInPrimaryView = receiverVisibleInPrimaryView;
    m_planarMirrorSurfaces.push_back(std::move(marker));
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
