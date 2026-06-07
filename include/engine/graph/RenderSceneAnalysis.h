#pragma once

#include "engine/graph/RenderGraphTypes.h"

#include <optional>
#include <cstddef>
#include <string>
#include <vector>

namespace engine::graph {
  /**
    * Scene-derived facts used during render graph compilation.
    *
    * RenderIntent describes the requested result. RenderSceneAnalysis describes
    * what the current scene snapshot contains or can support, so the compiler
    * can synthesize graph nodes from intent without treating pass topology as
    * user-authored input.
    */
  class RenderSceneAnalysis {
  public:
    struct SceneSurfaceMarker {
      std::string surfaceId;
      std::string surfaceName;
    };

    RenderSceneAnalysis();

    /**
      * Builds an analysis for callers that have not provided scene facts yet.
      *
      * Unknown counts are treated as "possibly present" so existing direct
      * compiler callers keep their previous conservative behavior.
      */
    static RenderSceneAnalysis unknownScene();

    void recordVisibleSurface();
    void recordVisibleLight();
    void recordPortalReceiverSurface(std::string surfaceId, std::string surfaceName);
    void recordPlanarMirrorSurface(std::string surfaceId, std::string surfaceName);

    bool hasKnownVisibleSurfaceCount() const;
    bool hasKnownVisibleLightCount() const;
    std::size_t visibleSurfaceCount() const;
    std::size_t visibleLightCount() const;
    std::size_t portalReceiverSurfaceCount() const;
    std::size_t planarMirrorSurfaceCount() const;

    const std::vector<SceneSurfaceMarker>& portalReceiverSurfaces() const;
    const std::vector<SceneSurfaceMarker>& planarMirrorSurfaces() const;

    bool hasVisibleSurfaces() const;
    bool hasVisibleLights() const;

    bool shouldCompileRasterPreviewShadows(RenderExecutorKind executor,
                                           const RenderIntent& intent) const;

  private:
    std::optional<std::size_t> m_visibleSurfaceCount{0};
    std::optional<std::size_t> m_visibleLightCount{0};
    std::vector<SceneSurfaceMarker> m_portalReceiverSurfaces;
    std::vector<SceneSurfaceMarker> m_planarMirrorSurfaces;
    bool m_rasterShadowMapsSupported{true};
  };
}
