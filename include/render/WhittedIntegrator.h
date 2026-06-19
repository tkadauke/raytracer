#pragma once

#include "render/Integrator.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class HitPoint;

namespace render {
  class Material;
  class Primitive;
  class WavefrontIntersectionBackend;
  struct WhittedContinuation;

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
    std::uint64_t estimatedIntersectionRaysPerPrimarySample() const override;
    std::uint64_t estimatedClosestHitRaysPerPrimarySample() const override;
    std::uint64_t estimatedAnyHitRaysPerPrimarySample() const override;

    void setMaximumRecursionDepth(int depth) override;
    int maximumRecursionDepth() const;

    void setCancellationCallback(CancellationCallback callback) override;

  private:
    class ActiveQueuedHits;
    class ActiveSampleTracker;
    struct BatchDepthMetrics;
    class DirectLightContributionBatch;
    class DirectLightVisibilityBatch;
    class ClosestHitQueuedRayFrontierBatch;
    struct QueuedHit;
    struct QueuedRay;
    class QueuedRayFrontier;
    class SampleColorBuffer;

    bool isCancelled() const;
    State continuationState(const State& parent, double throughput) const;
    bool queuedRayShouldTrace(const QueuedRay& queued) const;
    void recordQueuedRayTermination(const Scene& scene, QueuedRay& queued,
                                    SampleColorBuffer& result, const std::string& event) const;
    void recordQueuedRayMiss(const Scene& scene, QueuedRay& queued, SampleColorBuffer& result,
                             BatchDepthMetrics& depthMetrics) const;
    std::size_t partitionTraceableQueuedRays(QueuedRayFrontier& current) const;
    void intersectQueuedRayScalar(const WavefrontIntersectionBackend& intersectionBackend,
                                  const Scene& scene, QueuedRayFrontier& current,
                                  std::size_t queuedIndex, ActiveQueuedHits& activeHits,
                                  SampleColorBuffer& result, BatchDepthMetrics& depthMetrics,
                                  IntegratorBatchMetrics* metrics) const;
    void intersectQueuedRayPacket(const WavefrontIntersectionBackend& intersectionBackend,
                                  const Scene& scene, QueuedRayFrontier& current,
                                  std::size_t firstQueuedIndex, std::size_t laneCount,
                                  ActiveQueuedHits& activeHits, SampleColorBuffer& result,
                                  BatchDepthMetrics& depthMetrics,
                                  IntegratorBatchMetrics* metrics) const;
    void intersectQueuedRayPacket8(const WavefrontIntersectionBackend& intersectionBackend,
                                   const Scene& scene, QueuedRayFrontier& current,
                                   std::size_t firstQueuedIndex, std::size_t laneCount,
                                   ActiveQueuedHits& activeHits, SampleColorBuffer& result,
                                   BatchDepthMetrics& depthMetrics,
                                   IntegratorBatchMetrics* metrics) const;
    void intersectQueuedRayBatch(const WavefrontIntersectionBackend& intersectionBackend,
                                 const Scene& scene, QueuedRayFrontier& current,
                                 std::size_t traceableCount, ActiveQueuedHits& activeHits,
                                 SampleColorBuffer& result, BatchDepthMetrics& depthMetrics,
                                 IntegratorBatchMetrics* metrics) const;
    void intersectActiveFrontier(const WavefrontIntersectionBackend& intersectionBackend,
                                 const Scene& scene, QueuedRayFrontier& current,
                                 ActiveQueuedHits& activeHits, SampleColorBuffer& result,
                                 BatchDepthMetrics& depthMetrics,
                                 IntegratorBatchMetrics* metrics) const;
    void queueOrResolveContinuation(const Scene& scene, const WhittedContinuation& continuation,
                                    const QueuedRay& parent, QueuedRayFrontier& next,
                                    SampleColorBuffer& result,
                                    ActiveSampleTracker& nextActiveSamples) const;
    void shadeQueuedHit(const Scene& scene, const RayCaster& recursiveRayCaster,
                        const QueuedHit& hit, QueuedRayFrontier& current, QueuedRayFrontier& next,
                        SampleColorBuffer& result, ActiveSampleTracker& nextActiveSamples,
                        IntegratorBatchMetrics* metrics) const;
    bool canUseBatchedLocalWhittedDirectLighting(const Material& material, const Rayd& ray,
                                                 const HitPoint& hitPoint) const;
    int m_maximumRecursionDepth;
    CancellationCallback m_cancellationCallback;
  };
}
