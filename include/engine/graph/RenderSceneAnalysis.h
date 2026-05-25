#pragma once

#include "engine/graph/RenderGraphTypes.h"

#include <optional>
#include <cstddef>

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

    bool hasKnownVisibleSurfaceCount() const;
    bool hasKnownVisibleLightCount() const;
    std::size_t visibleSurfaceCount() const;
    std::size_t visibleLightCount() const;

    bool hasVisibleSurfaces() const;
    bool hasVisibleLights() const;

    bool shouldCompileRasterPreviewShadows(RenderExecutorKind executor,
                                           const RenderIntent& intent) const;

  private:
    std::optional<std::size_t> m_visibleSurfaceCount{0};
    std::optional<std::size_t> m_visibleLightCount{0};
    bool m_rasterShadowMapsSupported{true};
  };
}
