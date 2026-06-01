#pragma once

#include "render/Integrator.h"

#include <functional>

namespace render {

  /**
    * @brief Recursive Whitted single-ray radiance evaluator.
    *
    * `WhittedIntegrator` contains the light-transport policy historically
    * implemented directly by `engine::raytracer::Raytracer::rayColor`: recurse
    * into materials, return the scene background on misses and recursion
    * truncation, and return black for primitives without a material.
    */
  class WhittedIntegrator final : public Integrator {
  public:
    WhittedIntegrator();

    std::unique_ptr<Integrator> clone() const override;
    const char* diagnosticName() const override;
    const char* batchExecutionMode() const override;

    Colord radiance(const Scene& scene, const Rayd& ray, State& state,
                    const RayCaster& recursiveRayCaster) const override;

    std::vector<Colord> radianceBatch(const Scene& scene,
                                      const std::vector<IntegratorRaySample>& samples,
                                      const RayCaster& recursiveRayCaster,
                                      IntegratorBatchMetrics* metrics = nullptr,
                                      const IntegratorBatchSettings& settings = {}) const override;

    void setMaximumRecursionDepth(int depth) override;
    int maximumRecursionDepth() const;

    void setCancellationCallback(CancellationCallback callback) override;

  private:
    bool isCancelled() const;
    State continuationState(const State& parent, double throughput) const;

    int m_maximumRecursionDepth;
    CancellationCallback m_cancellationCallback;
  };
}
