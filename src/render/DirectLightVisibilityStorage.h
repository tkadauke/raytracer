#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "CountArithmetic.h"
#include "render/Integrator.h"
#include "render/WavefrontIntersectionBackend.h"

namespace render::detail {
  /// Selection/shadow-query/occlusion storage and the resolve/validate/metrics
  /// plumbing shared by the batch integrators' direct-light visibility
  /// batches. Each integrator derives its own `DirectLightVisibilityBatch`
  /// from this and adds the selection-collection and contribution logic that
  /// is specific to its algorithm.
  ///
  /// `Traits` supplies the integrator-specific diagnostic messages as static
  /// `const char*` members: `missingAnyHitFrontier`, `recordedEmptyWithQueries`,
  /// `occlusionSelectionMismatch`, and `occlusionFrontierMismatch`.
  template<typename Selection, typename Traits>
  class DirectLightVisibilityStorage {
  public:
    [[nodiscard]] bool empty() const {
      return m_selections.empty();
    }

    void recordEmptyVisibility(int depth, IntegratorBatchMetrics* metrics) const {
      if (!empty()) {
        throw std::logic_error(Traits::recordedEmptyWithQueries);
      }
      recordVisibilityDepth(depth, /*batchChunks=*/0, /*batchRays=*/0, /*packedRayBytes=*/0,
                            /*hostPackedRayBytes=*/0, /*hostQueryBytes=*/0,
                            /*stateHandleBytes=*/0, metrics);
    }

    void resolveOcclusion(const Scene& scene,
                          const WavefrontIntersectionBackend& intersectionBackend, int depth,
                          IntegratorBatchMetrics* metrics) {
      m_occluded.clear();
      m_frontier.reset();
      WavefrontDirectLightVisibilityBatchResult result =
        intersectionBackend.resolveDirectLightVisibilityBatch(scene, std::move(m_shadowQueries));
      m_frontier = std::move(result.frontier);
      m_occluded = std::move(result.occluded);
      requirePresent(m_frontier, Traits::missingAnyHitFrontier);
      validateResolvedFrontierRayCount();
      validateResolvedOcclusionCount();
      if (metrics) {
        metrics->recordDirectLightAnyHitFrontierQuery(depthIndex(depth), hostSelectionBytes(),
                                                      hostOcclusionBytes(), intersectionBackend,
                                                      *m_frontier, result.timing);
      }
    }

  protected:
    explicit DirectLightVisibilityStorage(std::size_t reserveCount) {
      m_selections.reserve(reserveCount);
      m_shadowQueries.reserve(reserveCount);
    }

    [[nodiscard]] std::uint64_t hostSelectionBytes() const {
      return static_cast<std::uint64_t>(m_selections.size()) * sizeof(Selection);
    }

    [[nodiscard]] std::uint64_t hostOcclusionBytes() const {
      return static_cast<std::uint64_t>(m_occluded.size()) *
             sizeof(WavefrontOcclusionFlags::value_type);
    }

    void recordVisibilityDepth(int depth, std::uint64_t batchChunks, std::uint64_t batchRays,
                               std::uint64_t packedRayBytes, std::uint64_t hostPackedRayBytes,
                               std::uint64_t hostQueryBytes, std::uint64_t stateHandleBytes,
                               IntegratorBatchMetrics* metrics) const {
      if (!metrics) {
        return;
      }
      metrics->recordDirectLightVisibilityDepth(depthIndex(depth), hostSelectionBytes(),
                                                hostOcclusionBytes(), batchChunks, batchRays,
                                                packedRayBytes, hostPackedRayBytes, hostQueryBytes,
                                                stateHandleBytes);
    }

    static std::uint64_t depthIndex(int depth) {
      return static_cast<std::uint64_t>(std::max(0, depth));
    }

    void validateResolvedOcclusionCount() const {
      validateResultCount(m_occluded.size(), m_selections.size(),
                          Traits::occlusionSelectionMismatch);
    }

    void validateResolvedFrontierRayCount() const {
      if (m_frontier) {
        validateResultCount(m_occluded.size(), m_frontier->rayCount(),
                            Traits::occlusionFrontierMismatch);
      }
    }

    /// Bounds-checked occlusion flag lookup without count validation.
    [[nodiscard]] bool occludedFlag(std::size_t index) const {
      return index < m_occluded.size() && m_occluded[index] != 0U;
    }

    std::vector<Selection> m_selections;
    std::vector<WavefrontAnyHitQuery> m_shadowQueries;
    std::unique_ptr<WavefrontAnyHitFrontier> m_frontier;
    WavefrontOcclusionFlags m_occluded;
  };
}
