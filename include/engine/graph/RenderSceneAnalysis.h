#pragma once

#include "engine/graph/RenderGraphTypes.h"

#include <optional>
#include <cstddef>
#include <set>
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

    struct SelectableSubset {
      std::string id;
      std::string label;
      SceneSelector selector;
      std::size_t elementCount{0};
    };

    enum class SelectorMatchStatus { Matched, Missing, Ambiguous };

    struct SelectorMatch {
      SelectorMatchStatus status{SelectorMatchStatus::Missing};
      const SelectableSubset* subset{nullptr};
      std::vector<const SelectableSubset*> candidates;

      bool matched() const;
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
    void recordRenderTextureReceiver(std::string subviewName);
    void recordSelectableObject(std::string objectId, std::string objectName,
                                std::vector<std::string> tags = {},
                                std::vector<std::string> layers = {},
                                std::string label = {});

    bool hasKnownVisibleSurfaceCount() const;
    bool hasKnownVisibleLightCount() const;
    bool hasKnownSelectableSubsets() const;
    std::size_t visibleSurfaceCount() const;
    std::size_t visibleLightCount() const;
    std::size_t portalReceiverSurfaceCount() const;
    std::size_t planarMirrorSurfaceCount() const;

    const std::vector<SceneSurfaceMarker>& portalReceiverSurfaces() const;
    const std::vector<SceneSurfaceMarker>& planarMirrorSurfaces() const;
    const std::set<std::string>& renderTextureSubviewReceivers() const;

    bool hasVisibleSurfaces() const;
    bool hasVisibleLights() const;

    bool shouldCompileRasterPreviewShadows(RenderExecutorKind executor,
                                           const RenderIntent& intent) const;
    const std::vector<SelectableSubset>& selectableSubsets() const;
    SelectorMatch matchSelector(const SceneSelector& selector) const;
    void requireResolvableSelectors(const RenderIntent& intent,
                                    const std::string& context) const;

  private:
    SelectableSubset& recordSubset(std::string id, std::string label,
                                   SceneSelector selector);
    std::vector<const SelectableSubset*> candidatesFor(const SceneSelector& selector) const;

    std::optional<std::size_t> m_visibleSurfaceCount{0};
    std::optional<std::size_t> m_visibleLightCount{0};
    std::vector<SceneSurfaceMarker> m_portalReceiverSurfaces;
    std::vector<SceneSurfaceMarker> m_planarMirrorSurfaces;
    std::set<std::string> m_renderTextureSubviewReceivers;
    bool m_rasterShadowMapsSupported{true};
    bool m_hasKnownSelectableSubsets{true};
    std::vector<SelectableSubset> m_selectableSubsets;
  };
}
