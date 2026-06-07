#pragma once

#include "render/Integrator.h"

#include <algorithm>
#include <cstddef>
#include <functional>

class HitPoint;

namespace render {
  class Light;
  class LightSampler;
  class Material;
  struct MaterialBsdfSample;
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
    *  2. Miss → add `throughput · background` while the path is still a
    *     camera/specular chain, otherwise add explicit environment radiance;
    *     terminate.
    *  3. Material doesn't support BSDF sampling → fall back to
    *     `Material::shade(...)` (Whitted compatibility), terminate.
    *  4. Add the material's compatibility ambient radiance.
    *  5. Direct lighting (next-event estimation): select
    *     `directLightSamples()` lights from `LightSampler`, draw one
    *     `LightSample` from each light's `SampleDimension::Light` slot,
    *     shadow-test, and average
    *     `throughput · BSDF.eval(wi, wo_light) · L_i / (pdf_light · pdf_select)`.
    *  6. Indirect: if the material publishes enumerable delta branches
    *     (`Material::deltaBsdfSamples(...)`), split them exactly; otherwise
    *     use `Material::sampleBsdf(...)` for one sampled continuation and
    *     update `throughput · value / pdf · |cos|`.
    *  7. Termination: sampled BSDF continuations use Russian roulette beyond
    *     `russianRouletteDepth()` and rescale surviving throughput. Exact
    *     enumerable delta branches use the deterministic Whitted throughput
    *     cutoff instead, because they are split branches rather than
    *     probability-compensated samples.
    *
    * Materials that don't yet expose a BSDF terminate the path with the
    * Whitted-shaded value; those surfaces don't yet receive indirect light
    * from the path tracer. Refactoring those materials to expose `sampleBsdf`
    * is follow-up work tracked alongside this integrator.
    *
    * @see WhittedIntegrator — the recursive direct-lighting-only
    * sibling.
    * @see SampleStream — the per-pixel sample provider; the path
    * tracer consumes `SampleDimension::BSDF`, `LightSelection`, `Light`, and
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
      m_russianRouletteDepth = std::max(1, depth);
    }

    /// Number of next-event-estimation light samples drawn at each
    /// non-delta hit. The samples are averaged, so increasing this
    /// reduces direct-light variance without changing the estimator's
    /// expected value. Default 1.
    int directLightSamples() const {
      return m_directLightSamples;
    }
    void setDirectLightSamples(int samples) {
      m_directLightSamples = std::max(1, samples);
    }

  private:
    struct BatchDepthMetrics;
    struct BatchHit;
    struct BatchPath;
    struct DirectLightingSample;
    struct ScalarPath;

    bool isCancelled() const;
    State clonePathState(const State& state) const;
    Colord missRadiance(const Scene& scene, bool backgroundVisible) const;
    double lightSelectionSample(State& state, int bounce, int directSampleIndex) const;
    Vector2d lightSample(State& state, int bounce, std::size_t lightIndex,
                         int directSampleIndex) const;
    Colord sampleDirectLighting(const Scene& scene, const LightSampler& lightSampler,
                                const HitPoint& hitPoint, const Material& material,
                                const Vector3d& wi, State& state, int bounce,
                                IntegratorBatchMetrics* metrics = nullptr) const;
    Colord emittedRadiance(const LightSampler& lightSampler, const Material& material,
                           const Rayd& ray, const HitPoint& hitPoint, bool sampledFromBsdf,
                           double bsdfSamplePdf, bool bsdfSampleDelta,
                           IntegratorBatchMetrics* metrics = nullptr) const;
    DirectLightingSample directLighting(const Scene& scene, const Light& light,
                                        const HitPoint& hitPoint, const Material& material,
                                        const Vector3d& wi, const Vector2d& lightSample,
                                        State& state) const;
    bool canContinueWithSample(const MaterialBsdfSample& sample, const HitPoint& hitPoint) const;
    Colord continuedThroughput(const Colord& throughput, const MaterialBsdfSample& sample,
                               const HitPoint& hitPoint) const;
    bool continuesExactDeltaBranch(const Colord& throughput) const;
    void setStateThroughput(State& state, const Colord& throughput) const;
    bool survivesRussianRoulette(Colord& throughput, State& state, int bounce) const;
    void recordDepthDelta(BatchDepthMetrics& depthMetrics, const Colord& before,
                          const Colord& after) const;
    void recordFrontierHit(std::size_t pathIndex, BatchPath& path, const Primitive& primitive,
                           const HitPoint& hitPoint, int bounce, BatchDepthMetrics& depthMetrics,
                           std::vector<BatchHit>& activeHits) const;
    void recordFrontierMiss(const Scene& scene, BatchPath& path, BatchDepthMetrics& depthMetrics,
                            const Colord& accumulatedBeforeDepth) const;
    void intersectActivePathScalar(const Scene& scene, std::size_t pathIndex,
                                   std::vector<BatchPath>& paths, std::vector<BatchHit>& activeHits,
                                   int bounce, BatchDepthMetrics& depthMetrics,
                                   IntegratorBatchMetrics* metrics) const;
    void intersectActivePathPacket(const Scene& scene, std::size_t firstPathIndex,
                                   std::size_t laneCount, std::vector<BatchPath>& paths,
                                   std::vector<BatchHit>& activeHits, int bounce,
                                   BatchDepthMetrics& depthMetrics,
                                   IntegratorBatchMetrics* metrics) const;
    void intersectActivePathPacket8(const Scene& scene, std::size_t firstPathIndex,
                                    std::size_t laneCount, std::vector<BatchPath>& paths,
                                    std::vector<BatchHit>& activeHits, int bounce,
                                    BatchDepthMetrics& depthMetrics,
                                    IntegratorBatchMetrics* metrics) const;
    void intersectActiveFrontier(const Scene& scene, std::vector<BatchPath>& paths,
                                 std::vector<BatchHit>& activeHits, int bounce,
                                 BatchDepthMetrics& depthMetrics,
                                 IntegratorBatchMetrics* metrics) const;
    void retainActivePath(std::vector<BatchPath>& paths, std::size_t pathIndex,
                          std::size_t& retainedPathCount) const;

    int m_maximumRecursionDepth{8};
    int m_russianRouletteDepth{3};
    int m_directLightSamples{1};
    CancellationCallback m_cancellationCallback;
  };
}
