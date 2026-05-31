#include "render/Integrator.h"

#include "render/State.h"

namespace render {

  std::vector<Colord> Integrator::radianceBatch(const Scene& scene,
                                                const std::vector<IntegratorRaySample>& samples,
                                                const RayCaster& recursiveRayCaster) const {
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
