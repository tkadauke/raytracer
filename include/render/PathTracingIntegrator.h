#pragma once

#include "render/Integrator.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>

class HitPoint;

namespace render {
  class Light;
  class LightSampler;
  class Material;
  struct MaterialBsdfSample;
  class PathMaterialTransport;
  class Primitive;
  class WavefrontFrontierCompactionResult;
  class WavefrontIntersectionBackend;

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
    * The scalar entry point walks a small list of active path branches
    * iteratively. The batch entry point uses the same transport rules but
    * groups active rays by bounce depth so scene traversal can use packet
    * intersections before each shading step. Both forms remain a single
    * integrator policy: they do not hand recursion back to materials.
    *
    * Algorithm at each bounce:
    *
    *  1. Trace `ray` into the scene.
    *  2. Miss → add `throughput · background` while the path is still a
    *     camera/specular chain, otherwise add explicit environment radiance;
    *     terminate.
    *  3. Material is not path-traceable → record the unsupported-material
    *     diagnostic and terminate. The path tracer does not call the
    *     material's Whitted `shade(...)` fallback.
    *  4. Add the material's local ambient radiance.
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
    * Materials that don't yet expose path-tracing transport terminate the path
    * explicitly. Refactoring those materials to expose `supportsPathTracing`
    * plus emitted/scattering contracts is follow-up work tracked alongside
    * this integrator.
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
    bool prefersProgressiveSamplePublishing() const override;
    std::uint64_t estimatedIntersectionRaysPerPrimarySample() const override;
    std::uint64_t estimatedClosestHitRaysPerPrimarySample() const override;
    std::uint64_t estimatedAnyHitRaysPerPrimarySample() const override;

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
    class ActivePathHits;
    class ClosestHitPathFrontierBatch;
    class DirectLightContributionBatch;
    struct DirectLightingCandidate;
    struct DirectLightingSelection;
    struct DirectLightingSample;
    class DirectLightVisibilityBatch;
    class HostBatchPathFrontier;
    struct PathContinuationState;
    class SampleColorBuffer;
    struct ScalarPath;

    bool isCancelled() const;
    Colord missRadiance(const Scene& scene, bool backgroundVisible) const;
    double lightSelectionSample(State& state, int bounce, int directSampleIndex) const;
    Vector2d lightSample(State& state, int bounce, std::size_t lightIndex,
                         int directSampleIndex) const;
    Colord sampleDirectLighting(const Scene& scene, const LightSampler& lightSampler,
                                const HitPoint& hitPoint, const PathMaterialTransport& material,
                                const Vector3d& wi, State& state, int bounce,
                                const WavefrontIntersectionBackend* intersectionBackend = nullptr,
                                IntegratorBatchMetrics* metrics = nullptr) const;
    DirectLightContributionBatch
    sampleDirectLightingBatch(const Scene& scene, const LightSampler& lightSampler,
                              const ActivePathHits& activeHits, HostBatchPathFrontier& paths,
                              int bounce, const WavefrontIntersectionBackend& intersectionBackend,
                              IntegratorBatchMetrics* metrics = nullptr) const;
    void recordDirectLightContributionExecution(const WavefrontIntersectionBackend& backend,
                                                IntegratorBatchMetrics* metrics) const;
    Colord emittedRadiance(const LightSampler& lightSampler, const PathMaterialTransport& material,
                           const Rayd& ray, const HitPoint& hitPoint, bool sampledFromBsdf,
                           double bsdfSamplePdf, bool bsdfSampleDelta,
                           IntegratorBatchMetrics* metrics = nullptr) const;
    DirectLightingCandidate directLightingCandidate(const Light& light, const HitPoint& hitPoint,
                                                    const Vector2d& lightSample) const;
    DirectLightingSample resolveDirectLightingCandidate(const DirectLightingCandidate& candidate,
                                                        const PathMaterialTransport& material,
                                                        const HitPoint& hitPoint,
                                                        const Vector3d& wi, bool occluded,
                                                        State& state) const;
    bool canContinueWithSample(const MaterialBsdfSample& sample, const HitPoint& hitPoint) const;
    Colord continuedThroughput(const Colord& throughput, const MaterialBsdfSample& sample,
                               const HitPoint& hitPoint) const;
    bool continuesExactDeltaBranch(const Colord& throughput) const;
    void setStateThroughput(State& state, const Colord& throughput) const;
    void recordUnsupportedPathMaterial(State& state,
                                       IntegratorBatchMetrics* metrics = nullptr) const;
    bool survivesRussianRoulette(Colord& throughput, State& state, int bounce) const;
    bool prepareSampledContinuation(const MaterialBsdfSample& sample, const HitPoint& hitPoint,
                                    PathContinuationState& continuation, State& state,
                                    int bounce) const;
    bool prepareExactDeltaContinuation(const MaterialBsdfSample& sample, const HitPoint& hitPoint,
                                       PathContinuationState& continuation, State& state) const;
    void recordDepthDelta(BatchDepthMetrics& depthMetrics, const Colord& before,
                          const Colord& after) const;
    void recordCancelledDepthMetrics(int bounce, IntegratorBatchMetrics& metrics) const;
    void recordFrontierHit(std::size_t pathIndex, BatchPath& path, const Primitive& primitive,
                           const HitPoint& hitPoint, int bounce, BatchDepthMetrics& depthMetrics,
                           ActivePathHits& activeHits) const;
    void recordFrontierMiss(const Scene& scene, BatchPath& path, BatchDepthMetrics& depthMetrics,
                            const Colord& accumulatedBeforeDepth) const;
    void intersectActivePathScalar(const WavefrontIntersectionBackend& intersectionBackend,
                                   const Scene& scene, std::size_t pathIndex,
                                   HostBatchPathFrontier& paths, ActivePathHits& activeHits,
                                   int bounce, BatchDepthMetrics& depthMetrics,
                                   IntegratorBatchMetrics* metrics) const;
    void intersectActivePathPacket(const WavefrontIntersectionBackend& intersectionBackend,
                                   const Scene& scene, std::size_t firstPathIndex,
                                   std::size_t laneCount, HostBatchPathFrontier& paths,
                                   ActivePathHits& activeHits, int bounce,
                                   BatchDepthMetrics& depthMetrics,
                                   IntegratorBatchMetrics* metrics) const;
    void intersectActivePathPacket8(const WavefrontIntersectionBackend& intersectionBackend,
                                    const Scene& scene, std::size_t firstPathIndex,
                                    std::size_t laneCount, HostBatchPathFrontier& paths,
                                    ActivePathHits& activeHits, int bounce,
                                    BatchDepthMetrics& depthMetrics,
                                    IntegratorBatchMetrics* metrics) const;
    void intersectActiveFrontierBatch(const WavefrontIntersectionBackend& intersectionBackend,
                                      const Scene& scene, HostBatchPathFrontier& paths,
                                      ActivePathHits& activeHits, int bounce,
                                      BatchDepthMetrics& depthMetrics,
                                      IntegratorBatchMetrics* metrics) const;
    void intersectActiveFrontier(const WavefrontIntersectionBackend& intersectionBackend,
                                 const Scene& scene, HostBatchPathFrontier& paths,
                                 ActivePathHits& activeHits, int bounce,
                                 BatchDepthMetrics& depthMetrics,
                                 IntegratorBatchMetrics* metrics) const;
    bool shadeActiveHit(const Scene& scene, const LightSampler& lightSampler,
                        const ActivePathHits& activeHits, std::size_t hitIndex,
                        HostBatchPathFrontier& paths, HostBatchPathFrontier& spawnedPaths,
                        const DirectLightContributionBatch& directLightContributions, int bounce,
                        BatchDepthMetrics& depthMetrics, IntegratorBatchMetrics* metrics) const;

    int m_maximumRecursionDepth{8};
    int m_russianRouletteDepth{3};
    int m_directLightSamples{1};
    CancellationCallback m_cancellationCallback;
  };
}
