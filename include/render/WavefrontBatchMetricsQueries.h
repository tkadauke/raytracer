#pragma once

#include "core/util/VectorUtil.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace render {
  /**
   * Derived-metric queries shared by every wavefront batch-metrics record.
   *
   * `IntegratorBatchMetrics` and the wavefront render report's batch summary
   * carry the same per-depth counters; both derive from this CRTP mixin so the
   * compaction-candidate, query-round-trip, and mixed-query-depth accessors are
   * defined once. `Metrics` must expose the counter members these queries read.
   */
  template<class Metrics>
  struct WavefrontBatchMetricsQueries {
    [[nodiscard]] double frontierCompactionRemovedSampleFraction() const {
      if (self().frontierCompactionInputSamples == 0) {
        return 0.0;
      }
      return static_cast<double>(self().frontierCompactionRemovedSamples) /
             static_cast<double>(self().frontierCompactionInputSamples);
    }

    [[nodiscard]] double frontierCompactionMovedRetainedSampleFraction() const {
      if (self().frontierCompactionRetainedSamples == 0) {
        return 0.0;
      }
      return static_cast<double>(self().frontierCompactionMovedSamples) /
             static_cast<double>(self().frontierCompactionRetainedSamples);
    }

    [[nodiscard]] bool hasCompactionCandidateDepth(std::size_t depth) const {
      if (depth >= self().activeSamplesPerDepth.size() ||
          depth >= self().retainedActiveSamplesPerDepth.size()) {
        return false;
      }
      return self().activeSamplesPerDepth[depth] > self().retainedActiveSamplesPerDepth[depth];
    }

    [[nodiscard]] std::uint64_t compactionCandidateSamplesAtDepth(std::size_t depth) const {
      if (!hasCompactionCandidateDepth(depth)) {
        return 0;
      }
      return self().activeSamplesPerDepth[depth] - self().retainedActiveSamplesPerDepth[depth];
    }

    [[nodiscard]] std::uint64_t compactionCandidateDepthCount() const {
      const std::size_t depthCount =
        std::min(self().activeSamplesPerDepth.size(), self().retainedActiveSamplesPerDepth.size());
      std::uint64_t count = 0;
      for (std::size_t depth = 0; depth != depthCount; ++depth) {
        if (hasCompactionCandidateDepth(depth)) {
          ++count;
        }
      }
      return count;
    }

    [[nodiscard]] std::uint64_t compactionCandidateSampleCount() const {
      const std::size_t depthCount =
        std::min(self().activeSamplesPerDepth.size(), self().retainedActiveSamplesPerDepth.size());
      std::uint64_t count = 0;
      for (std::size_t depth = 0; depth != depthCount; ++depth) {
        count += compactionCandidateSamplesAtDepth(depth);
      }
      return count;
    }

    [[nodiscard]] std::uint64_t compactionCandidateHostPathStateBytes() const {
      const std::size_t depthCount = std::min(self().activeHostPathStateBytesPerDepth.size(),
                                              self().retainedHostPathStateBytesPerDepth.size());
      std::uint64_t bytes = 0;
      for (std::size_t depth = 0; depth != depthCount; ++depth) {
        if (self().activeHostPathStateBytesPerDepth[depth] >
            self().retainedHostPathStateBytesPerDepth[depth]) {
          bytes += self().activeHostPathStateBytesPerDepth[depth] -
                   self().retainedHostPathStateBytesPerDepth[depth];
        }
      }
      return bytes;
    }

    [[nodiscard]] double compactionCandidateSampleFraction() const {
      if (self().activeSampleDepthsProcessed == 0) {
        return 0.0;
      }
      return static_cast<double>(compactionCandidateSampleCount()) /
             static_cast<double>(self().activeSampleDepthsProcessed);
    }

    [[nodiscard]] std::uint64_t largestCompactionCandidateDepth() const {
      const std::size_t depthCount =
        std::min(self().activeSamplesPerDepth.size(), self().retainedActiveSamplesPerDepth.size());
      std::uint64_t largestDepth = 0;
      std::uint64_t largestSamples = 0;
      for (std::size_t depth = 0; depth != depthCount; ++depth) {
        const std::uint64_t samples = compactionCandidateSamplesAtDepth(depth);
        if (samples > largestSamples) {
          largestSamples = samples;
          largestDepth = static_cast<std::uint64_t>(depth);
        }
      }
      return largestDepth;
    }

    [[nodiscard]] std::uint64_t largestCompactionCandidateSampleCount() const {
      const std::size_t depthCount =
        std::min(self().activeSamplesPerDepth.size(), self().retainedActiveSamplesPerDepth.size());
      std::uint64_t largestSamples = 0;
      for (std::size_t depth = 0; depth != depthCount; ++depth) {
        largestSamples = std::max(largestSamples, compactionCandidateSamplesAtDepth(depth));
      }
      return largestSamples;
    }

    [[nodiscard]] std::uint64_t largestCompactionCandidateHostPathStateBytes() const {
      const std::uint64_t depth = largestCompactionCandidateDepth();
      if (depth >= self().activeHostPathStateBytesPerDepth.size() ||
          depth >= self().retainedHostPathStateBytesPerDepth.size() ||
          self().activeHostPathStateBytesPerDepth[depth] <=
            self().retainedHostPathStateBytesPerDepth[depth]) {
        return 0;
      }
      return self().activeHostPathStateBytesPerDepth[depth] -
             self().retainedHostPathStateBytesPerDepth[depth];
    }

    [[nodiscard]] double largestCompactionCandidateSampleFraction() const {
      const std::uint64_t samples = largestCompactionCandidateSampleCount();
      if (samples == 0) {
        return 0.0;
      }
      const std::uint64_t depth = largestCompactionCandidateDepth();
      if (depth >= self().activeSamplesPerDepth.size() ||
          self().activeSamplesPerDepth[depth] == 0) {
        return 0.0;
      }
      return static_cast<double>(samples) /
             static_cast<double>(self().activeSamplesPerDepth[depth]);
    }

    [[nodiscard]] bool hasMixedQueryDepth(std::size_t depth) const {
      const std::uint64_t closestHitChunks =
        depth < self().frontierClosestHitBatchChunksPerDepth.size()
          ? self().frontierClosestHitBatchChunksPerDepth[depth]
          : 0;
      const std::uint64_t anyHitChunks = depth < self().directLightAnyHitBatchChunksPerDepth.size()
                                           ? self().directLightAnyHitBatchChunksPerDepth[depth]
                                           : 0;
      return closestHitChunks > 0 && anyHitChunks > 0;
    }

    [[nodiscard]] std::uint64_t frontierQueryRoundTrips() const {
      std::uint64_t roundTrips = 0;
      for (const std::uint64_t chunks : self().frontierClosestHitBatchChunksPerDepth) {
        roundTrips += chunks;
      }
      for (const std::uint64_t chunks : self().directLightAnyHitBatchChunksPerDepth) {
        roundTrips += chunks;
      }
      return roundTrips;
    }

    [[nodiscard]] std::uint64_t residentFrontierQueryRoundTripsEstimate() const {
      return frontierQueryRoundTrips() - residentFrontierQueryRoundTripSavingsEstimate();
    }

    [[nodiscard]] std::uint64_t residentFrontierQueryRoundTripSavingsEstimate() const {
      const std::uint64_t mixedRoundTrips = mixedQueryDepthRoundTrips();
      const std::uint64_t mixedResidentBoundaries = mixedQueryDepthCount();
      if (mixedRoundTrips <= mixedResidentBoundaries) {
        return 0;
      }
      return mixedRoundTrips - mixedResidentBoundaries;
    }

    [[nodiscard]] std::uint64_t directLightAnyHitQueryRoundTrips() const {
      std::uint64_t roundTrips = 0;
      for (const std::uint64_t chunks : self().directLightAnyHitBatchChunksPerDepth) {
        roundTrips += chunks;
      }
      return roundTrips;
    }

    [[nodiscard]] std::uint64_t residentDirectLightBatchRoundTripsEstimate() const {
      return directLightAnyHitQueryRoundTrips() -
             residentDirectLightBatchRoundTripSavingsEstimate();
    }

    [[nodiscard]] std::uint64_t residentDirectLightBatchRoundTripSavingsEstimate() const {
      return directLightAnyHitQueryRoundTrips();
    }

    [[nodiscard]] bool hasResidentDirectLightBatchCandidateDepth(std::size_t depth) const {
      return core::util::valueAt(self().directLightAnyHitBatchRaysPerDepth, depth) > 0;
    }

    [[nodiscard]] std::uint64_t residentDirectLightBatchCandidateDepthCount() const {
      std::uint64_t count = 0;
      for (std::size_t depth = 0; depth != self().directLightAnyHitBatchRaysPerDepth.size();
           ++depth) {
        if (hasResidentDirectLightBatchCandidateDepth(depth)) {
          ++count;
        }
      }
      return count;
    }

    [[nodiscard]] std::uint64_t residentDirectLightBatchCandidateRayCount() const {
      std::uint64_t count = 0;
      for (const std::uint64_t rays : self().directLightAnyHitBatchRaysPerDepth) {
        count += rays;
      }
      return count;
    }

    [[nodiscard]] std::uint64_t residentDirectLightBatchHostBytesAtDepth(std::size_t depth) const {
      return core::util::valueAt(self().directLightSelectionHostBytesPerDepth, depth) +
             core::util::valueAt(self().directLightOcclusionHostBytesPerDepth, depth) +
             core::util::valueAt(self().directLightContributionHostBytesPerDepth, depth) +
             core::util::valueAt(self().directLightAnyHitFrontierHostPackedRayBytesPerDepth,
                                 depth) +
             core::util::valueAt(self().directLightAnyHitFrontierHostQueryBytesPerDepth, depth) +
             core::util::valueAt(self().directLightAnyHitFrontierStateHandleBytesPerDepth, depth);
    }

    [[nodiscard]] std::uint64_t residentDirectLightBatchCandidateHostBytes() const {
      const std::size_t depthCount =
        std::max({self().directLightSelectionHostBytesPerDepth.size(),
                  self().directLightOcclusionHostBytesPerDepth.size(),
                  self().directLightContributionHostBytesPerDepth.size(),
                  self().directLightAnyHitFrontierHostPackedRayBytesPerDepth.size(),
                  self().directLightAnyHitFrontierHostQueryBytesPerDepth.size(),
                  self().directLightAnyHitFrontierStateHandleBytesPerDepth.size()});
      std::uint64_t bytes = 0;
      for (std::size_t depth = 0; depth != depthCount; ++depth) {
        bytes += residentDirectLightBatchHostBytesAtDepth(depth);
      }
      return bytes;
    }

    [[nodiscard]] std::uint64_t largestResidentDirectLightBatchDepth() const {
      std::uint64_t largestDepth = 0;
      std::uint64_t largestRays = 0;
      for (std::size_t depth = 0; depth != self().directLightAnyHitBatchRaysPerDepth.size();
           ++depth) {
        const std::uint64_t rays = self().directLightAnyHitBatchRaysPerDepth[depth];
        if (rays > largestRays) {
          largestRays = rays;
          largestDepth = static_cast<std::uint64_t>(depth);
        }
      }
      return largestDepth;
    }

    [[nodiscard]] std::uint64_t largestResidentDirectLightBatchRayCount() const {
      std::uint64_t largestRays = 0;
      for (const std::uint64_t rays : self().directLightAnyHitBatchRaysPerDepth) {
        largestRays = std::max(largestRays, rays);
      }
      return largestRays;
    }

    [[nodiscard]] std::uint64_t largestResidentDirectLightBatchPackedRayBytes() const {
      return core::util::valueAt(self().directLightAnyHitFrontierPackedRayBytesPerDepth,
                                 largestResidentDirectLightBatchDepth());
    }

    [[nodiscard]] std::uint64_t largestResidentDirectLightBatchHostBytes() const {
      const std::uint64_t depth = largestResidentDirectLightBatchDepth();
      if (largestResidentDirectLightBatchRayCount() == 0) {
        return 0;
      }
      return residentDirectLightBatchHostBytesAtDepth(static_cast<std::size_t>(depth));
    }

    [[nodiscard]] std::uint64_t mixedQueryDepthCount() const {
      const std::size_t depthCount = std::max(self().frontierClosestHitBatchChunksPerDepth.size(),
                                              self().directLightAnyHitBatchChunksPerDepth.size());
      std::uint64_t count = 0;
      for (std::size_t depth = 0; depth != depthCount; ++depth) {
        if (hasMixedQueryDepth(depth)) {
          ++count;
        }
      }
      return count;
    }

    [[nodiscard]] std::uint64_t mixedQueryDepthRoundTrips() const {
      const std::size_t depthCount = std::max(self().frontierClosestHitBatchChunksPerDepth.size(),
                                              self().directLightAnyHitBatchChunksPerDepth.size());
      std::uint64_t roundTrips = 0;
      for (std::size_t depth = 0; depth != depthCount; ++depth) {
        if (hasMixedQueryDepth(depth)) {
          if (depth < self().frontierClosestHitBatchChunksPerDepth.size()) {
            roundTrips += self().frontierClosestHitBatchChunksPerDepth[depth];
          }
          if (depth < self().directLightAnyHitBatchChunksPerDepth.size()) {
            roundTrips += self().directLightAnyHitBatchChunksPerDepth[depth];
          }
        }
      }
      return roundTrips;
    }

    [[nodiscard]] std::uint64_t mixedQueryDepthRays() const {
      return mixedQueryDepthClosestHitRays() + mixedQueryDepthAnyHitRays();
    }

    [[nodiscard]] std::uint64_t mixedQueryDepthClosestHitRays() const {
      const std::size_t depthCount = std::max(self().frontierClosestHitBatchRaysPerDepth.size(),
                                              self().directLightAnyHitBatchChunksPerDepth.size());
      std::uint64_t rays = 0;
      for (std::size_t depth = 0; depth != depthCount; ++depth) {
        if (hasMixedQueryDepth(depth) &&
            depth < self().frontierClosestHitBatchRaysPerDepth.size()) {
          rays += self().frontierClosestHitBatchRaysPerDepth[depth];
        }
      }
      return rays;
    }

    [[nodiscard]] std::uint64_t mixedQueryDepthAnyHitRays() const {
      const std::size_t depthCount = std::max(self().frontierClosestHitBatchChunksPerDepth.size(),
                                              self().directLightAnyHitBatchRaysPerDepth.size());
      std::uint64_t rays = 0;
      for (std::size_t depth = 0; depth != depthCount; ++depth) {
        if (hasMixedQueryDepth(depth) && depth < self().directLightAnyHitBatchRaysPerDepth.size()) {
          rays += self().directLightAnyHitBatchRaysPerDepth[depth];
        }
      }
      return rays;
    }

    [[nodiscard]] std::uint64_t mixedQueryDepthReadbackBytes() const {
      return self().mixedQueryDepthClosestHitReadbackBytes() +
             self().mixedQueryDepthAnyHitReadbackBytes();
    }

  private:
    const Metrics& self() const {
      return static_cast<const Metrics&>(*this);
    }
  };
}
