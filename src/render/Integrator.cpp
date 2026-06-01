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

  std::vector<Colord> Integrator::radianceBatch(const Scene& scene,
                                                const std::vector<IntegratorRaySample>& samples,
                                                const RayCaster& recursiveRayCaster,
                                                IntegratorBatchMetrics* metrics,
                                                const IntegratorBatchSettings& settings) const {
    if (metrics) {
      metrics->usedScalarFallback = true;
      metrics->activeSamplesPerDepth.clear();
      metrics->radianceDeltaSquaredSumPerDepth.clear();
      metrics->maxRadianceDeltaPerDepth.clear();
      metrics->stoppedByConvergence = false;
      metrics->stoppedAfterDepth = 0;
    }

    std::vector<Colord> result;
    result.reserve(samples.size());

    double deltaSquaredSum = 0.0;
    double maxDelta = 0.0;
    for (const auto& sample : samples) {
      State state;
      state.timeSample = sample.timeSample;
      state.sampleStream = sample.sampleStream.get();
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
        metrics->activeSamplesPerDepth.push_back(samples.size());
        metrics->radianceDeltaSquaredSumPerDepth.push_back(deltaSquaredSum);
        metrics->maxRadianceDeltaPerDepth.push_back(maxDelta);
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
