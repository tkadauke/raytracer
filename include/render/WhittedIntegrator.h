#pragma once

#include "render/Integrator.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class HitPoint;

namespace render {
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

    void setMaximumRecursionDepth(int depth) override;
    int maximumRecursionDepth() const;

    void setCancellationCallback(CancellationCallback callback) override;

  private:
    struct BatchDepthMetrics;
    struct QueuedHit;
    struct QueuedRay;

    bool isCancelled() const;
    State continuationState(const State& parent, double throughput) const;
    bool queuedRayShouldTrace(const QueuedRay& queued) const;
    void recordQueuedRayTermination(const Scene& scene, QueuedRay& queued,
                                    std::vector<Colord>& result, const std::string& event) const;
    void recordQueuedRayMiss(const Scene& scene, QueuedRay& queued, std::vector<Colord>& result,
                             BatchDepthMetrics& depthMetrics) const;
    void clearActiveSampleMarks(std::vector<unsigned char>& sampleMarks,
                                const std::vector<std::size_t>& activeSampleIndices) const;
    void markActiveSample(std::vector<unsigned char>& sampleMarks,
                          std::vector<std::size_t>& activeSampleIndices,
                          std::size_t sampleIndex) const;
    std::uint64_t collectCurrentActiveSamples(const std::vector<QueuedRay>& current,
                                              std::vector<unsigned char>& activeSamples,
                                              std::vector<std::size_t>& activeSampleIndices) const;
    std::size_t partitionTraceableQueuedRays(std::vector<QueuedRay>& current) const;
    void intersectQueuedRayScalar(const WavefrontIntersectionBackend& intersectionBackend,
                                  const Scene& scene, std::vector<QueuedRay>& current,
                                  std::size_t queuedIndex, std::vector<QueuedHit>& activeHits,
                                  std::vector<Colord>& result, BatchDepthMetrics& depthMetrics,
                                  IntegratorBatchMetrics* metrics) const;
    void intersectQueuedRayPacket(const WavefrontIntersectionBackend& intersectionBackend,
                                  const Scene& scene, std::vector<QueuedRay>& current,
                                  std::size_t firstQueuedIndex, std::size_t laneCount,
                                  std::vector<QueuedHit>& activeHits, std::vector<Colord>& result,
                                  BatchDepthMetrics& depthMetrics,
                                  IntegratorBatchMetrics* metrics) const;
    void intersectQueuedRayPacket8(const WavefrontIntersectionBackend& intersectionBackend,
                                   const Scene& scene, std::vector<QueuedRay>& current,
                                   std::size_t firstQueuedIndex, std::size_t laneCount,
                                   std::vector<QueuedHit>& activeHits, std::vector<Colord>& result,
                                   BatchDepthMetrics& depthMetrics,
                                   IntegratorBatchMetrics* metrics) const;
    void intersectActiveFrontier(const WavefrontIntersectionBackend& intersectionBackend,
                                 const Scene& scene, std::vector<QueuedRay>& current,
                                 std::vector<QueuedHit>& activeHits, std::vector<Colord>& result,
                                 BatchDepthMetrics& depthMetrics,
                                 IntegratorBatchMetrics* metrics) const;
    void prepareContinuationQueue(std::vector<QueuedRay>& next, std::size_t currentQueueSize) const;
    void queueOrResolveContinuation(const Scene& scene, const WhittedContinuation& continuation,
                                    const QueuedRay& parent, std::vector<QueuedRay>& next,
                                    std::vector<Colord>& result,
                                    std::vector<unsigned char>& nextActiveSamples,
                                    bool countNextActiveSamples,
                                    std::vector<std::size_t>& nextActiveSampleIndices) const;
    void shadeQueuedHit(const Scene& scene, const RayCaster& recursiveRayCaster,
                        const QueuedHit& hit, std::vector<QueuedRay>& current,
                        std::vector<QueuedRay>& next, std::vector<Colord>& result,
                        std::vector<unsigned char>& nextActiveSamples, bool countNextActiveSamples,
                        std::vector<std::size_t>& nextActiveSampleIndices,
                        IntegratorBatchMetrics* metrics) const;

    int m_maximumRecursionDepth;
    CancellationCallback m_cancellationCallback;
  };
}
