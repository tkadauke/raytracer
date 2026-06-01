#pragma once

#include "render/Integrator.h"

#include <cstddef>
#include <functional>

class HitPoint;

namespace render {
  class Light;
  class Material;
  class Primitive;

  /**
    * @brief Iterative megakernel Monte Carlo path tracer.
    *
    * A `Integrator` subclass that consumes the BSDF / Light sampling
    * substrate (`BSDF::sample`, `Material::sampleBsdf`, `Light::sample`)
    * and the deterministic per-pixel sample stream
    * (`State::sampleStream`) to compute incoming radiance along a
    * primary ray as
    *
    *   `L(x, ω) = Σ_bounces throughput · (direct lighting + recursive radiance)`
    *
    * The classic megakernel shape: one function walks the entire path
    * iteratively, no separate stages, no ray queue. Simpler than a
    * wavefront tracer and the canonical teaching algorithm. See
    * `docs/plans/whitted-ray-packets.md` for why packets and wavefront
    * are deliberately out of scope here.
    *
    * Algorithm at each bounce:
    *
    *  1. Trace `ray` into the scene.
    *  2. Miss → add `throughput · background`, terminate.
    *  3. Material doesn't support BSDF sampling → fall back to
    *     `Material::shade(...)` (Whitted compatibility), terminate.
    *  4. Direct lighting (next-event estimation): for each light, draw
    *     a `LightSample`, shadow-test, accumulate
    *     `throughput · BSDF.eval(wi, wo_light) · L_i / pdf_light`.
    *  5. Indirect: `Material::sampleBsdf(...)` for the next direction
    *     and BSDF value, update `throughput · value / pdf · |cos|`,
    *     advance the ray.
    *  6. Russian roulette beyond `russianRouletteDepth()` — terminate
    *     with probability `1 - throughput.max()`; otherwise rescale
    *     throughput to compensate.
    *
    * Materials that don't yet expose a BSDF terminate the path with the
    * Whitted-shaded value; those surfaces don't yet receive indirect light
    * from the path tracer. Refactoring those materials to expose `sampleBsdf`
    * is follow-up work tracked alongside this integrator.
    *
    * @see WhittedIntegrator — the recursive direct-lighting-only
    * sibling.
    * @see SampleStream — the per-pixel sample provider; the path
    * tracer consumes `SampleDimension::BSDF`, `Light`, and
    * `Continuation` per bounce.
    */
  class PathTracingIntegrator : public Integrator {
  public:
    PathTracingIntegrator();

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

    void setCancellationCallback(CancellationCallback callback) override;

    /// Maximum number of bounces along a single path. Includes the
    /// primary ray. Default 8.
    int maximumRecursionDepth() const {
      return m_maximumRecursionDepth;
    }
    void setMaximumRecursionDepth(int depth) override {
      m_maximumRecursionDepth = depth;
    }

    /// Bounce depth at which Russian-roulette termination starts. RR
    /// rolls a per-bounce coin against `throughput.max()`; survivors
    /// have their throughput rescaled to keep the estimator unbiased.
    /// Default 3.
    int russianRouletteDepth() const {
      return m_russianRouletteDepth;
    }
    void setRussianRouletteDepth(int depth) {
      m_russianRouletteDepth = depth;
    }

  private:
    struct BatchDepthMetrics;
    struct BatchHit;
    struct BatchPath;

    bool isCancelled() const;
    Colord directLighting(const Scene& scene, const Light& light, const HitPoint& hitPoint,
                          const Material& material, const Vector3d& wi, State& state) const;
    void recordDepthDelta(BatchDepthMetrics& depthMetrics, const Colord& before,
                          const Colord& after) const;
    void intersectActiveFrontier(const Scene& scene,
                                 const std::vector<std::size_t>& activePathIndices,
                                 std::vector<BatchPath>& paths, std::vector<BatchHit>& activeHits,
                                 int bounce, BatchDepthMetrics& depthMetrics,
                                 IntegratorBatchMetrics* metrics) const;

    int m_maximumRecursionDepth{8};
    int m_russianRouletteDepth{3};
    CancellationCallback m_cancellationCallback;
  };
}
