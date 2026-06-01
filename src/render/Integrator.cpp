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
    frontierScalarRaysPerDepth.clear();
    activeSampleDepthsProcessed = 0;
    radianceDeltaSquaredSumPerDepth.clear();
    maxRadianceDeltaPerDepth.clear();
    compatibilityShadeSamples = 0;
    stoppedByConvergence = false;
    stoppedAfterDepth = 0;
    intersectionWorkerSeconds = 0.0;
    shadingWorkerSeconds = 0.0;
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
                                                       std::uint64_t scalarRays) {
    frontierPacketChunksPerDepth.push_back(packetChunks);
    frontierScalarRaysPerDepth.push_back(scalarRays);
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
        settings.progressObserver->depthCompleted(/*completedDepth=*/1, result, samples.size());
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
