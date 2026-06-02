#include "render/Integrator.h"

#include "render/State.h"

#include <algorithm>
#include <cmath>

namespace render {
  const char* Integrator::diagnosticName() const {
    return "custom";
  }

  const char* Integrator::batchExecutionMode() const {
    return "scalar_loop";
  }

  void IntegratorBatchMetrics::reset(bool scalarFallback) {
    usedScalarFallback = scalarFallback;
    activeSamplesPerDepth.clear();
    frontierRayHitsPerDepth.clear();
    frontierRayMissesPerDepth.clear();
    frontierPacketChunksPerDepth.clear();
    frontierPacketRaysPerDepth.clear();
    frontierScalarRaysPerDepth.clear();
    frontierPacketScalarFallbackRaysPerDepth.clear();
    frontierPacketScalarFallbackRaysByReason.clear();
    frontierPacketRefinedRaysPerDepth.clear();
    frontierPacketRefinedRaysByMaterial.clear();
    activeSampleDepthsProcessed = 0;
    radianceDeltaSquaredSumPerDepth.clear();
    maxRadianceDeltaPerDepth.clear();
    compatibilityShadeSamples = 0;
    stoppedByConvergence = false;
    stoppedAfterDepth = 0;
    intersectionWorkerSeconds = 0.0;
    shadingWorkerSeconds = 0.0;
    pathSetupWorkerSeconds = 0.0;
    frontierBookkeepingWorkerSeconds = 0.0;
    progressSnapshotWorkerSeconds = 0.0;
    convergenceTestWorkerSeconds = 0.0;
    observerConvergenceFeedbackDepths = 0;
  }

  void IntegratorBatchMetrics::recordActiveDepth(std::uint64_t activeSamples) {
    activeSamplesPerDepth.push_back(activeSamples);
    activeSampleDepthsProcessed += activeSamples;
  }

  void IntegratorBatchMetrics::recordFrontierIntersections(std::uint64_t hitRays,
                                                           std::uint64_t missRays) {
    frontierRayHitsPerDepth.push_back(hitRays);
    frontierRayMissesPerDepth.push_back(missRays);
  }

  void IntegratorBatchMetrics::recordFrontierTraversal(std::uint64_t packetChunks,
                                                       std::uint64_t packetRays,
                                                       std::uint64_t scalarRays,
                                                       std::uint64_t packetScalarFallbackRays,
                                                       std::uint64_t packetRefinedRays) {
    frontierPacketChunksPerDepth.push_back(packetChunks);
    frontierPacketRaysPerDepth.push_back(packetRays);
    frontierScalarRaysPerDepth.push_back(scalarRays);
    frontierPacketScalarFallbackRaysPerDepth.push_back(packetScalarFallbackRays);
    frontierPacketRefinedRaysPerDepth.push_back(packetRefinedRays);
  }

  void IntegratorBatchMetrics::recordPacketScalarFallbacksByReason(
    const std::map<std::string, std::uint64_t>& reasons) {
    for (const auto& [reason, count] : reasons) {
      const std::string label = reason.empty() ? "unknown" : reason;
      frontierPacketScalarFallbackRaysByReason[label] += count;
    }
  }

  void IntegratorBatchMetrics::recordPacketHitRefinement(const std::string& materialLabel) {
    const std::string label = materialLabel.empty() ? "unknown" : materialLabel;
    ++frontierPacketRefinedRaysByMaterial[label];
  }

  void IntegratorBatchMetrics::recordRadianceDeltaDepth(double squaredSum, double maxDelta) {
    radianceDeltaSquaredSumPerDepth.push_back(squaredSum);
    maxRadianceDeltaPerDepth.push_back(maxDelta);
  }

  std::vector<Colord> Integrator::radianceBatch(const Scene& scene,
                                                const std::vector<IntegratorRaySample>& samples,
                                                const RayCaster& recursiveRayCaster,
                                                IntegratorBatchMetrics* metrics,
                                                const IntegratorBatchSettings& settings) const {
    if (metrics) {
      metrics->reset(/*scalarFallback=*/true);
    }

    std::vector<Colord> result;
    result.reserve(samples.size());

    double deltaSquaredSum = 0.0;
    double maxDelta = 0.0;
    for (const auto& sample : samples) {
      State state;
      state.timeSample = sample.timeSample;
      state.sampleStream = sample.sampleStream();
      const Colord color = radiance(scene, sample.ray, state, recursiveRayCaster);
      if (metrics) {
        const double deltaSquared = radianceDeltaSquared(Colord::black(), color);
        deltaSquaredSum += deltaSquared;
        maxDelta = std::max(maxDelta, std::sqrt(deltaSquared));
      }
      result.push_back(color);
    }

    if (!samples.empty()) {
      if (metrics) {
        metrics->recordActiveDepth(samples.size());
        metrics->recordRadianceDeltaDepth(deltaSquaredSum, maxDelta);
      }
      if (settings.progressObserver) {
        (void)settings.progressObserver->depthCompleted(/*completedDepth=*/1, result,
                                                        samples.size());
      }
    }

    return result;
  }

  void Integrator::setMaximumRecursionDepth(int) {
  }

  void Integrator::setCancellationCallback(CancellationCallback) {
  }

  double Integrator::radianceDeltaSquared(const Colord& before, const Colord& after) const {
    const Colord delta = after - before;
    return delta.r() * delta.r() + delta.g() * delta.g() + delta.b() * delta.b();
  }
}
