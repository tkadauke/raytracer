#include "render/Integrator.h"

#include "render/State.h"

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
                                                IntegratorBatchMetrics* metrics) const {
    if (metrics) {
      metrics->usedScalarFallback = true;
      metrics->activeSamplesPerDepth.clear();
      if (!samples.empty()) {
        metrics->activeSamplesPerDepth.push_back(samples.size());
      }
    }

    std::vector<Colord> result;
    result.reserve(samples.size());

    for (const auto& sample : samples) {
      State state;
      state.timeSample = sample.timeSample;
      state.sampleStream = sample.sampleStream.get();
      result.push_back(radiance(scene, sample.ray, state, recursiveRayCaster));
    }

    return result;
  }

  void Integrator::setMaximumRecursionDepth(int) {
  }

  void Integrator::setCancellationCallback(CancellationCallback) {
  }
}
