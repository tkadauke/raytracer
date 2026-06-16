#include "engine/graph/RenderSceneAnalysis.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace engine::graph {
  RenderSceneAnalysis::RenderSceneAnalysis() = default;

  bool RenderSceneAnalysis::SelectorMatch::matched() const {
    return status == SelectorMatchStatus::Matched && subset != nullptr;
  }

  RenderSceneAnalysis RenderSceneAnalysis::unknownScene() {
    RenderSceneAnalysis analysis;
    analysis.m_visibleSurfaceCount.reset();
    analysis.m_visibleLightCount.reset();
    analysis.m_hasKnownSelectableSubsets = false;
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

  void RenderSceneAnalysis::recordRenderTextureReceiver(std::string subviewName) {
    if (!subviewName.empty()) {
      m_renderTextureSubviewReceivers.insert(std::move(subviewName));
    }
  }

  void RenderSceneAnalysis::recordSelectableObject(std::string objectId, std::string objectName,
                                                   std::vector<std::string> tags,
                                                   std::vector<std::string> layers,
                                                   std::string label) {
    const std::string displayLabel =
      label.empty() ? (!objectName.empty() ? objectName : objectId) : label;

    if (!objectId.empty()) {
      auto& subset =
        recordSubset("object_id:" + objectId, displayLabel, SceneSelector::objectId(objectId));
      ++subset.elementCount;
    }
    if (!objectName.empty()) {
      auto& subset = recordSubset("object_name:" + objectName, objectName,
                                  SceneSelector::objectName(objectName));
      ++subset.elementCount;
    }
    for (const auto& tag : tags) {
      if (tag.empty()) {
        continue;
      }
      auto& subset = recordSubset("tag:" + tag, "Tag: " + tag, SceneSelector::tag(tag));
      ++subset.elementCount;
    }
    for (const auto& layer : layers) {
      if (layer.empty()) {
        continue;
      }
      auto& subset = recordSubset("layer:" + layer, "Layer: " + layer, SceneSelector::layer(layer));
      ++subset.elementCount;
    }
  }

  void RenderSceneAnalysis::setFullGpuTracingSupported(bool supported, std::string reason) {
    m_fullGpuTracingSupported = supported;
    m_fullGpuTracingUnsupportedReason =
      supported ? std::string()
                : (reason.empty() ? "full GPU tracing subset is not implemented for this scene"
                                  : std::move(reason));
  }

  void RenderSceneAnalysis::setFullGpuTracingBackendAvailable(bool available, std::string reason) {
    m_fullGpuTracingBackendAvailable = available;
    m_fullGpuTracingBackendUnavailableReason =
      available
        ? std::string()
        : (reason.empty() ? "full GPU tracing backend is not available" : std::move(reason));
  }

  bool RenderSceneAnalysis::hasKnownVisibleSurfaceCount() const {
    return m_visibleSurfaceCount.has_value();
  }

  bool RenderSceneAnalysis::hasKnownVisibleLightCount() const {
    return m_visibleLightCount.has_value();
  }

  bool RenderSceneAnalysis::hasKnownSelectableSubsets() const {
    return m_hasKnownSelectableSubsets;
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

  bool RenderSceneAnalysis::fullGpuTracingSupported() const {
    return m_fullGpuTracingSupported;
  }

  bool RenderSceneAnalysis::fullGpuTracingBackendAvailable() const {
    return m_fullGpuTracingBackendAvailable;
  }

  const std::string& RenderSceneAnalysis::fullGpuTracingUnsupportedReason() const {
    return m_fullGpuTracingUnsupportedReason;
  }

  const std::string& RenderSceneAnalysis::fullGpuTracingBackendUnavailableReason() const {
    return m_fullGpuTracingBackendUnavailableReason;
  }

  bool RenderSceneAnalysis::shouldCompileRasterPreviewShadows(RenderExecutorKind executor,
                                                              const RenderIntent& intent) const {
    return intent.enablePreviewShadows && executor == RenderExecutorKind::Rasterizer &&
           m_rasterShadowMapsSupported && hasVisibleSurfaces() && hasVisibleLights();
  }

  const std::vector<RenderSceneAnalysis::SelectableSubset>&
  RenderSceneAnalysis::selectableSubsets() const {
    return m_selectableSubsets;
  }

  RenderSceneAnalysis::SelectorMatch
  RenderSceneAnalysis::matchSelector(const SceneSelector& selector) const {
    if (selector.selectsWholeFrame()) {
      static const SelectableSubset wholeFrame{"all", "All", SceneSelector::all(), 0};
      return {SelectorMatchStatus::Matched, &wholeFrame, {&wholeFrame}};
    }

    auto candidates = candidatesFor(selector);
    if (candidates.empty()) {
      return {SelectorMatchStatus::Missing, nullptr, {}};
    }

    const bool ambiguous = selector.kind == SceneSelector::Kind::ObjectId ||
                           selector.kind == SceneSelector::Kind::ObjectName;
    if (ambiguous && (candidates.size() > 1 || candidates.front()->elementCount > 1)) {
      return {SelectorMatchStatus::Ambiguous, nullptr, std::move(candidates)};
    }

    return {SelectorMatchStatus::Matched, candidates.front(), std::move(candidates)};
  }

  void RenderSceneAnalysis::requireResolvableSelectors(const RenderIntent& intent,
                                                       const std::string& context) const {
    if (!hasKnownSelectableSubsets()) {
      return;
    }

    for (const auto& viewOverride : intent.selectorSpecificOverrides()) {
      const SelectorMatch match = matchSelector(viewOverride.selector);
      if (match.matched()) {
        continue;
      }

      std::ostringstream message;
      message << context << " cannot resolve scene selector "
              << viewOverride.selector.displayText();
      if (match.status == SelectorMatchStatus::Missing) {
        message << ": no visible scene subset matches it";
      } else {
        std::size_t objectCount = 0;
        for (const auto* candidate : match.candidates) {
          objectCount += candidate ? candidate->elementCount : 0;
        }
        message << ": selector is ambiguous across " << objectCount << " visible objects";
      }
      throw std::runtime_error(message.str());
    }
  }

  RenderSceneAnalysis::SelectableSubset&
  RenderSceneAnalysis::recordSubset(std::string id, std::string label, SceneSelector selector) {
    auto existing = std::find_if(
      m_selectableSubsets.begin(), m_selectableSubsets.end(), [&](const SelectableSubset& subset) {
        return subset.id == id && subset.selector.kind == selector.kind &&
               subset.selector.value == selector.value;
      });
    if (existing != m_selectableSubsets.end()) {
      return *existing;
    }

    m_selectableSubsets.push_back(
      SelectableSubset{std::move(id), std::move(label), std::move(selector), 0});
    return m_selectableSubsets.back();
  }

  std::vector<const RenderSceneAnalysis::SelectableSubset*>
  RenderSceneAnalysis::candidatesFor(const SceneSelector& selector) const {
    std::vector<const SelectableSubset*> result;
    for (const auto& subset : m_selectableSubsets) {
      if (subset.selector.kind == selector.kind && subset.selector.value == selector.value) {
        result.push_back(&subset);
      }
    }
    return result;
  }
}
