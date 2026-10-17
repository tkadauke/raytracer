#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>

#include "render/Integrator.h"
#include "render/State.h"

namespace render::detail {
  /// Per-depth frontier counters shared by the batch integrators. Each
  /// integrator derives its own `BatchDepthMetrics` from this and adds the
  /// state that is specific to its path representation.
  struct FrontierDepthMetrics {
    bool trackRadianceDelta{false};
    std::uint64_t frontierRayHits{0};
    std::uint64_t frontierRayMisses{0};
    std::uint64_t frontierPacketChunks{0};
    std::uint64_t frontierPacketRays{0};
    std::uint64_t frontierClosestHitBatchChunks{0};
    std::uint64_t frontierClosestHitBatchRays{0};
    std::uint64_t frontierRay4PacketChunks{0};
    std::uint64_t frontierRay8PacketChunks{0};
    std::uint64_t frontierScalarRays{0};
    std::uint64_t frontierPacketScalarFallbackRays{0};
    std::map<std::string, std::uint64_t> frontierPacketScalarFallbackRaysByReason;
    std::uint64_t frontierPacketRefinedRays{0};
    double depthDeltaSquaredSum{0.0};
    double depthMaxDelta{0.0};
    IntegratorBatchMetrics* metrics{nullptr};

    bool trackFrontierMetrics() const {
      return metrics != nullptr;
    }

    void recordResolvedFrontier(std::uint64_t activeHitHostBytes) const {
      if (!metrics) {
        return;
      }

      metrics->recordActiveHitHostBytes(activeHitHostBytes);
      metrics->recordFrontierIntersections(frontierRayHits, frontierRayMisses);
      metrics->recordFrontierTraversal(frontierPacketChunks, frontierPacketRays,
                                       frontierRay4PacketChunks, frontierRay8PacketChunks,
                                       frontierScalarRays, frontierPacketScalarFallbackRays,
                                       frontierPacketRefinedRays);
      metrics->recordFrontierClosestHitBatch(frontierClosestHitBatchChunks,
                                             frontierClosestHitBatchRays);
      metrics->recordPacketScalarFallbacksByReason(frontierPacketScalarFallbackRaysByReason);
    }

    void recordCancelledDepth(int depth) const {
      if (!metrics) {
        return;
      }

      const std::uint64_t depthIndex = static_cast<std::uint64_t>(std::max(0, depth));
      metrics->recordSkippedDepthDiagnostics(depthIndex);
    }

    void recordPacketScalarFallbacks(const State& state,
                                     const std::map<std::string, std::uint64_t>& reasonsBefore) {
      for (const auto& [reason, count] : state.packetHitScalarFallbacksByReason) {
        const auto before = reasonsBefore.find(reason);
        const std::uint64_t previous = before == reasonsBefore.end() ? 0 : before->second;
        if (count > previous) {
          const std::uint64_t delta = count - previous;
          frontierPacketScalarFallbackRays += delta;
          frontierPacketScalarFallbackRaysByReason[reason] += delta;
        }
      }
    }
  };
}
