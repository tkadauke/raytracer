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
    using CancellationCallback = std::function<bool()>;

    WhittedIntegrator();

    std::unique_ptr<Integrator> clone() const override;

    Colord radiance(const Scene& scene, const Rayd& ray, State& state,
                    const RayCaster& recursiveRayCaster) const override;

    void setMaximumRecursionDepth(int depth);
    int maximumRecursionDepth() const;

    void setCancellationCallback(CancellationCallback callback);

  private:
    bool isCancelled() const;

    int m_maximumRecursionDepth;
    CancellationCallback m_cancellationCallback;
  };
}
