#include "render/WhittedIntegrator.h"

#include "core/ScopeExit.h"
#include "core/math/Constants.h"
#include "core/math/HitPoint.h"
#include "core/math/HitPointInterval.h"
#include "core/util/ScopedTimer.h"
#include "render/RayCaster.h"
#include "render/State.h"
#include "render/WavefrontIntersectionBackend.h"
#include "render/lights/Light.h"
#include "render/materials/Material.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>

namespace render {

  WhittedIntegrator::WhittedIntegrator()
      : m_maximumRecursionDepth(10) {
  }

  std::unique_ptr<Integrator> WhittedIntegrator::clone() const {
    auto result = std::make_unique<WhittedIntegrator>();
    result->setMaximumRecursionDepth(m_maximumRecursionDepth);
    return result;
  }

  const char* WhittedIntegrator::diagnosticName() const {
    return "whitted";
  }

  const char* WhittedIntegrator::batchExecutionMode() const {
    return "depth_major_whitted";
  }

  std::uint64_t WhittedIntegrator::estimatedIntersectionRaysPerPrimarySample() const {
    return estimatedClosestHitRaysPerPrimarySample();
  }

  std::uint64_t WhittedIntegrator::estimatedClosestHitRaysPerPrimarySample() const {
    return static_cast<std::uint64_t>(std::max(1, m_maximumRecursionDepth));
  }

  std::uint64_t WhittedIntegrator::estimatedAnyHitRaysPerPrimarySample() const {
    return static_cast<std::uint64_t>(std::max(1, m_maximumRecursionDepth));
  }

  struct WhittedIntegrator::QueuedRay {
    std::size_t sampleIndex{0};
    Rayd ray;
    Colord weight{Colord::white()};
    State state;
  };

  class WhittedIntegrator::QueuedRayFrontier {
  public:
    void reserve(std::size_t count) {
      m_rays.reserve(count);
    }

    [[nodiscard]] std::size_t capacity() const {
      return m_rays.capacity();
    }

    void clear() {
      m_rays.clear();
    }

    void prepareForNextDepth(std::size_t expectedCount) {
      clear();
      if (capacity() < expectedCount) {
        reserve(expectedCount);
      }
    }

    [[nodiscard]] bool empty() const {
      return m_rays.empty();
    }

    [[nodiscard]] std::size_t size() const {
      return m_rays.size();
    }

    [[nodiscard]] std::uint64_t hostBytes() const {
      return static_cast<std::uint64_t>(m_rays.size()) * sizeof(QueuedRay);
    }

    void recordActiveHostPathStateBytes(IntegratorBatchMetrics* metrics) const {
      if (metrics) {
        metrics->recordActiveHostPathStateBytes(hostBytes());
      }
    }

    void recordRetainedHostPathStateBytes(IntegratorBatchMetrics* metrics) const {
      if (metrics) {
        metrics->recordRetainedHostPathStateBytes(hostBytes());
      }
    }

    void recordSpawnedContinuations(IntegratorBatchMetrics* metrics) const {
      if (metrics) {
        metrics->recordSpawnedContinuations(size(), hostBytes());
      }
    }

    [[nodiscard]] std::uint64_t
    retainedActiveSampleCount(bool countNextActiveSamples,
                              const std::vector<std::size_t>& nextActiveSampleIndices) const {
      if (countNextActiveSamples) {
        return static_cast<std::uint64_t>(nextActiveSampleIndices.size());
      }
      return static_cast<std::uint64_t>(size());
    }

    void recordCompletedDepth(std::uint64_t retainedActiveSamples,
                              IntegratorBatchMetrics* metrics) const {
      if (!metrics) {
        return;
      }
      recordSpawnedContinuations(metrics);
      metrics->recordRetainedActiveDepth(retainedActiveSamples);
      recordRetainedHostPathStateBytes(metrics);
    }

    void push(QueuedRay queued) {
      m_rays.push_back(std::move(queued));
    }

    void swap(QueuedRayFrontier& other) {
      m_rays.swap(other.m_rays);
    }

    void advanceTo(QueuedRayFrontier& next) {
      swap(next);
    }

    QueuedRay& operator[](std::size_t index) {
      return m_rays[index];
    }

    const QueuedRay& operator[](std::size_t index) const {
      return m_rays[index];
    }

    [[nodiscard]] std::vector<QueuedRay>::iterator begin() {
      return m_rays.begin();
    }

    [[nodiscard]] std::vector<QueuedRay>::iterator end() {
      return m_rays.end();
    }

    [[nodiscard]] std::vector<QueuedRay>::const_iterator begin() const {
      return m_rays.begin();
    }

    [[nodiscard]] std::vector<QueuedRay>::const_iterator end() const {
      return m_rays.end();
    }

  private:
    std::vector<QueuedRay> m_rays;
  };

  struct WhittedIntegrator::QueuedHit {
    std::size_t queuedIndex{0};
    const Primitive* primitive{nullptr};
    std::shared_ptr<Material> material;
    HitPoint hitPoint;
  };

  class WhittedIntegrator::ActiveQueuedHits {
  public:
    void reserve(std::size_t count) {
      m_hits.reserve(count);
    }

    void clear() {
      m_hits.clear();
    }

    [[nodiscard]] std::size_t size() const {
      return m_hits.size();
    }

    [[nodiscard]] bool empty() const {
      return m_hits.empty();
    }

    [[nodiscard]] std::uint64_t hostBytes() const {
      return static_cast<std::uint64_t>(m_hits.size()) * sizeof(QueuedHit);
    }

    const QueuedHit& operator[](std::size_t index) const {
      return m_hits[index];
    }

    void add(std::size_t queuedIndex, const Primitive* primitive,
             std::shared_ptr<Material> material, const HitPoint& hitPoint) {
      m_hits.push_back(QueuedHit{queuedIndex, primitive, std::move(material), hitPoint});
    }

    [[nodiscard]] std::vector<QueuedHit>::const_iterator begin() const {
      return m_hits.begin();
    }

    [[nodiscard]] std::vector<QueuedHit>::const_iterator end() const {
      return m_hits.end();
    }

    void shade(const WhittedIntegrator& integrator, const Scene& scene,
               const WavefrontIntersectionBackend& intersectionBackend,
               const RayCaster& recursiveRayCaster, int depth, QueuedRayFrontier& current,
               QueuedRayFrontier& next, std::vector<Colord>& result,
               std::vector<unsigned char>& nextActiveSamples, bool countNextActiveSamples,
               std::vector<std::size_t>& nextActiveSampleIndices,
               IntegratorBatchMetrics* metrics) const;

  private:
    std::vector<QueuedHit> m_hits;
  };

  struct WhittedIntegrator::BatchDepthMetrics {
    std::uint64_t frontierRayHits{0};
    std::uint64_t frontierRayMisses{0};
    std::uint64_t frontierPacketChunks{0};
    std::uint64_t frontierPacketRays{0};
    std::uint64_t frontierClosestHitBatchChunks{0};
    std::uint64_t frontierClosestHitBatchRays{0};
    std::uint64_t frontierRay4PacketChunks{0};
    std::uint64_t frontierRay8PacketChunks{0};
    std::uint64_t frontierScalarRays{0};
    std::uint64_t frontierPacketScalarFallbackRays{0};
    std::map<std::string, std::uint64_t> frontierPacketScalarFallbackRaysByReason;
    std::uint64_t frontierPacketRefinedRays{0};
    bool trackFrontierMetrics{false};

    void recordPacketScalarFallbacks(const State& state,
                                     const std::map<std::string, std::uint64_t>& reasonsBefore) {
      for (const auto& [reason, count] : state.packetHitScalarFallbacksByReason) {
        const auto before = reasonsBefore.find(reason);
        const std::uint64_t previous = before == reasonsBefore.end() ? 0 : before->second;
        if (count > previous) {
          const std::uint64_t delta = count - previous;
          frontierPacketScalarFallbackRays += delta;
          frontierPacketScalarFallbackRaysByReason[reason] += delta;
        }
      }
    }

    void publish(IntegratorBatchMetrics* metrics) const {
      if (!metrics) {
        return;
      }
      metrics->recordFrontierIntersections(frontierRayHits, frontierRayMisses);
      metrics->recordFrontierTraversal(frontierPacketChunks, frontierPacketRays,
                                       frontierRay4PacketChunks, frontierRay8PacketChunks,
                                       frontierScalarRays, frontierPacketScalarFallbackRays,
                                       frontierPacketRefinedRays);
      metrics->recordFrontierClosestHitBatch(frontierClosestHitBatchChunks,
                                             frontierClosestHitBatchRays);
      metrics->recordPacketScalarFallbacksByReason(frontierPacketScalarFallbackRaysByReason);
    }
  };

  class WhittedIntegrator::ClosestHitQueuedRayFrontierBatch {
  public:
    ClosestHitQueuedRayFrontierBatch(QueuedRayFrontier& current, std::size_t traceableCount,
                                     BatchDepthMetrics& depthMetrics,
                                     IntegratorBatchMetrics* metrics)
        : m_expectedHitCount(traceableCount) {
      m_queries.reserve(traceableCount);
      {
        core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds
                                              : nullptr);
        if (depthMetrics.trackFrontierMetrics) {
          ++depthMetrics.frontierClosestHitBatchChunks;
          depthMetrics.frontierClosestHitBatchRays += traceableCount;
        }
        for (std::size_t queuedIndex = 0; queuedIndex != traceableCount; ++queuedIndex) {
          QueuedRay& queued = current[queuedIndex];
          queued.state.recurseIn();
          m_queries.push_back(WavefrontClosestHitQuery{queued.ray, &queued.state});
        }
      }
    }

    void intersect(const Scene& scene, const WavefrontIntersectionBackend& intersectionBackend,
                   IntegratorBatchMetrics* metrics) {
      WavefrontIntersectionQueryTiming intersectionTiming;
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      m_frontier = intersectionBackend.createClosestHitFrontier(std::move(m_queries));
      m_hits =
        intersectionBackend.intersectClosestFrontier(scene, *m_frontier, &intersectionTiming);
      validateHitCount();
      if (metrics) {
        metrics->recordClosestHitFrontierResidency(
          m_frontier->residency(), m_frontier->packedRayBytes(), m_frontier->hostQueryBytes(),
          m_frontier->stateHandleBytes());
        metrics->recordClosestHitQuery(intersectionBackend, m_frontier->rayCount(),
                                       intersectionTiming);
      }
    }

    [[nodiscard]] const WavefrontClosestHitResult* hit(std::size_t queuedIndex) const {
      if (queuedIndex >= m_hits.size() || !m_hits[queuedIndex].hit()) {
        return nullptr;
      }
      return &m_hits[queuedIndex];
    }

  private:
    void validateHitCount() const {
      if (m_hits.size() != m_expectedHitCount) {
        throw std::logic_error(
          "Whitted closest-hit frontier returned a hit count that does not match its queued-ray "
          "count");
      }
    }

    std::size_t m_expectedHitCount{0};
    std::vector<WavefrontClosestHitQuery> m_queries;
    std::unique_ptr<WavefrontClosestHitFrontier> m_frontier;
    std::vector<WavefrontClosestHitResult> m_hits;
  };

  class WhittedIntegrator::DirectLightVisibilityBatch {
  public:
    struct Selection {
      std::size_t hitIndex{0};
      LightSample sample;
      Colord contribution{Colord::black()};
    };

    DirectLightVisibilityBatch(std::size_t activeHitCount, std::size_t reserveCount)
        : m_locallyShaded(activeHitCount, 0) {
      m_selections.reserve(reserveCount);
      m_shadowQueries.reserve(reserveCount);
    }

    void collectLocalDirectLighting(const WhittedIntegrator& integrator, const Scene& scene,
                                    const ActiveQueuedHits& activeHits, QueuedRayFrontier& current,
                                    std::vector<Colord>& result, IntegratorBatchMetrics* metrics);

    [[nodiscard]] bool empty() const {
      return m_selections.empty();
    }

    [[nodiscard]] bool locallyShaded(std::size_t hitIndex) const {
      if (hitIndex >= m_locallyShaded.size()) {
        throw std::logic_error("Whitted direct-light visibility batch queried an invalid active "
                               "hit index");
      }
      return m_locallyShaded[hitIndex] != 0U;
    }

    void recordEmptyVisibility(int depth, IntegratorBatchMetrics* metrics) const {
      if (!empty()) {
        throw std::logic_error("Whitted direct-light visibility batch recorded as empty while it "
                               "still contains shadow queries");
      }
      recordSelectionHostBytes(depth, metrics);
      recordDirectLightChunks(depth, 0, 0, 0, 0, 0, metrics);
      recordOcclusionHostBytes(depth, metrics);
    }

    void resolveOcclusion(const Scene& scene,
                          const WavefrontIntersectionBackend& intersectionBackend, int depth,
                          IntegratorBatchMetrics* metrics) {
      WavefrontIntersectionQueryTiming intersectionTiming;
      m_occluded.clear();
      m_frontier.reset();
      recordSelectionHostBytes(depth, metrics);
      if (intersectionBackend.prefersAnyHitBatch(m_shadowQueries.size())) {
        m_frontier = intersectionBackend.createAnyHitFrontier(std::move(m_shadowQueries));
        m_occluded = intersectionBackend.intersectAnyFrontier(
          scene, *m_frontier, metrics ? &intersectionTiming : nullptr);
        validateResolvedOcclusionCount();
        if (metrics) {
          recordDirectLightChunks(depth, 1, m_frontier->rayCount(), m_frontier->packedRayBytes(),
                                  m_frontier->hostQueryBytes(), m_frontier->stateHandleBytes(),
                                  metrics);
          metrics->recordAnyHitFrontierResidency(
            m_frontier->residency(), m_frontier->packedRayBytes(), m_frontier->hostQueryBytes(),
            m_frontier->stateHandleBytes());
          metrics->recordAnyHitQuery(intersectionBackend, m_frontier->rayCount(),
                                     intersectionTiming);
          recordOcclusionHostBytes(depth, metrics);
        }
        return;
      }

      m_occluded.reserve(m_shadowQueries.size());
      if (metrics) {
        const std::uint64_t queryCount = static_cast<std::uint64_t>(m_shadowQueries.size());
        recordDirectLightChunks(depth, queryCount, queryCount, 0,
                                queryCount * sizeof(WavefrontAnyHitQuery), 0, metrics);
      }
      for (const WavefrontAnyHitQuery& query : m_shadowQueries) {
        WavefrontIntersectionQueryTiming queryTiming;
        State scratchState;
        State& queryState = query.state ? *query.state : scratchState;
        const bool blocked = intersectionBackend.intersectAny(
          scene, query.ray, query.maxDistance, queryState, metrics ? &queryTiming : nullptr);
        m_occluded.push_back(blocked ? 1U : 0U);
        if (metrics) {
          metrics->recordAnyHitQuery(intersectionBackend, 1, queryTiming);
        }
      }
      validateResolvedOcclusionCount();
      recordOcclusionHostBytes(depth, metrics);
    }

    void recordContributionHostBytes(int depth, IntegratorBatchMetrics* metrics) const {
      if (metrics) {
        metrics->recordDirectLightContributionHostBytes(
          static_cast<std::uint64_t>(std::max(0, depth)),
          static_cast<std::uint64_t>(m_selections.size()) * sizeof(Colord));
      }
    }

    void applyResolvedContributions(const WavefrontIntersectionBackend& intersectionBackend,
                                    const ActiveQueuedHits& activeHits, QueuedRayFrontier& current,
                                    std::vector<Colord>& result, int depth,
                                    IntegratorBatchMetrics* metrics) const {
      validateResolvedOcclusionCount();
      if (metrics) {
        const std::string request =
          intersectionBackend.requestedName() ? intersectionBackend.requestedName() : "";
        metrics->recordDirectLightContributionExecution(
          "cpu", request == "gpu" ? "GPU Whitted direct-light contribution kernel unavailable"
                                  : std::string());
      }
      for (std::size_t selectionIndex = 0; selectionIndex != m_selections.size();
           ++selectionIndex) {
        const Selection& selection = m_selections[selectionIndex];
        const QueuedHit& hit = activeHits[selection.hitIndex];
        QueuedRay& queued = current[hit.queuedIndex];
        const bool blocked = occluded(selectionIndex);
        const bool contributing = !blocked && selection.contribution != Colord::black();
        if (metrics) {
          metrics->recordDirectLightSample(blocked, contributing);
        }
        if (!contributing) {
          continue;
        }
        const Colord weightedContribution = queued.weight * selection.contribution;
        result[queued.sampleIndex] += weightedContribution;
        if (metrics) {
          metrics->recordDirectLightRadiance(weightedContribution,
                                             queued.state.recursionDepth <= 1);
        }
      }
      recordContributionHostBytes(depth, metrics);
    }

  private:
    void add(std::size_t hitIndex, const LightSample& sample, const Colord& contribution,
             const HitPoint& hitPoint, State& state) {
      m_selections.push_back(Selection{hitIndex, sample, contribution});
      m_shadowQueries.push_back(WavefrontAnyHitQuery{
        Rayd(hitPoint.point(), sample.direction).epsilonShifted(), sample.distance, &state});
    }

    void markLocallyShaded(std::size_t hitIndex) {
      if (hitIndex >= m_locallyShaded.size()) {
        throw std::logic_error("Whitted direct-light visibility batch marked an invalid active "
                               "hit index");
      }
      m_locallyShaded[hitIndex] = 1;
    }

    [[nodiscard]] bool occluded(std::size_t index) const {
      validateResolvedOcclusionCount();
      return index < m_occluded.size() && m_occluded[index] != 0U;
    }

    [[nodiscard]] std::uint64_t hostSelectionBytes() const {
      return static_cast<std::uint64_t>(m_selections.size()) * sizeof(Selection);
    }

    [[nodiscard]] std::uint64_t hostOcclusionBytes() const {
      return static_cast<std::uint64_t>(m_occluded.size()) *
             sizeof(WavefrontOcclusionFlags::value_type);
    }

    void recordSelectionHostBytes(int depth, IntegratorBatchMetrics* metrics) const {
      if (metrics) {
        metrics->recordDirectLightSelectionHostBytes(static_cast<std::uint64_t>(std::max(0, depth)),
                                                     hostSelectionBytes());
      }
    }

    void recordOcclusionHostBytes(int depth, IntegratorBatchMetrics* metrics) const {
      if (metrics) {
        metrics->recordDirectLightOcclusionHostBytes(static_cast<std::uint64_t>(std::max(0, depth)),
                                                     hostOcclusionBytes());
      }
    }

    static void recordDirectLightChunks(int depth, std::uint64_t batchChunks,
                                        std::uint64_t batchRays, std::uint64_t packedRayBytes,
                                        std::uint64_t hostQueryBytes,
                                        std::uint64_t stateHandleBytes,
                                        IntegratorBatchMetrics* metrics) {
      if (metrics) {
        metrics->recordDirectLightAnyHitBatch(static_cast<std::uint64_t>(std::max(0, depth)),
                                              batchChunks, batchRays, packedRayBytes,
                                              hostQueryBytes, stateHandleBytes);
      }
    }

    void validateResolvedOcclusionCount() const {
      if (m_occluded.size() != m_selections.size()) {
        throw std::logic_error("Whitted direct-light visibility batch resolved an occlusion count "
                               "that does not match its light-selection count");
      }
    }

    std::vector<unsigned char> m_locallyShaded;
    std::vector<Selection> m_selections;
    std::vector<WavefrontAnyHitQuery> m_shadowQueries;
    std::unique_ptr<WavefrontAnyHitFrontier> m_frontier;
    WavefrontOcclusionFlags m_occluded;
  };

  Colord WhittedIntegrator::radiance(const Scene& scene, const Rayd& ray, State& state,
                                     const RayCaster& recursiveRayCaster) const {
    if (isCancelled()) {
      return scene.background();
    }

    state.recurseIn();
    ScopeExit sx([&] { state.recurseOut(); });

    if (state.recursionDepth == m_maximumRecursionDepth) {
      state.recordEvent(nullptr,
                        "Raytracer: maximum recursion depth reached, returning background");
      return scene.background();
    }

    if (state.throughput < RAYTRACER_THROUGHPUT_CUTOFF) {
      state.recordEvent(nullptr, "Raytracer: throughput below cutoff, returning background");
      return scene.background();
    }

    HitPointInterval hitPoints;
    const Primitive* primitive = scene.intersect(ray, hitPoints, state);
    if (isCancelled()) {
      return scene.background();
    }

    if (!primitive) {
      state.recordEvent(nullptr, "Raytracer: Nothing hit, returning background color");
      return scene.background();
    }

    const HitPoint hitPoint = hitPoints.minWithPositiveDistance();
    if (state.recursionDepth == 1) {
      state.hitPoint = hitPoint;
    }

    if (!primitive->material()) {
      state.recordEvent(nullptr, "Raytracer: no material found, returning black");
      return Colord::black();
    }

    state.recordEvent(nullptr, "Raytracer: shading material");
    return primitive->material()->shade(&recursiveRayCaster, scene, ray, hitPoint, state);
  }

  bool WhittedIntegrator::queuedRayShouldTrace(const QueuedRay& queued) const {
    return queued.state.recursionDepth + 1 != m_maximumRecursionDepth &&
           !(queued.state.throughput < RAYTRACER_THROUGHPUT_CUTOFF);
  }

  void WhittedIntegrator::recordQueuedRayTermination(const Scene& scene, QueuedRay& queued,
                                                     std::vector<Colord>& result,
                                                     const std::string& event) const {
    if (!event.empty()) {
      queued.state.recordEvent(nullptr, event);
    }
    result[queued.sampleIndex] += queued.weight * scene.background();
    queued.state.recurseOut();
  }

  void WhittedIntegrator::recordQueuedRayMiss(const Scene& scene, QueuedRay& queued,
                                              std::vector<Colord>& result,
                                              BatchDepthMetrics& depthMetrics) const {
    if (depthMetrics.trackFrontierMetrics) {
      ++depthMetrics.frontierRayMisses;
    }
    queued.state.recordEvent(nullptr, "Raytracer: Nothing hit, returning background color");
    result[queued.sampleIndex] += queued.weight * scene.background();
    queued.state.recurseOut();
  }

  void WhittedIntegrator::clearActiveSampleMarks(
    std::vector<unsigned char>& sampleMarks,
    const std::vector<std::size_t>& activeSampleIndices) const {
    for (const std::size_t sampleIndex : activeSampleIndices) {
      sampleMarks[sampleIndex] = 0;
    }
  }

  void WhittedIntegrator::markActiveSample(std::vector<unsigned char>& sampleMarks,
                                           std::vector<std::size_t>& activeSampleIndices,
                                           std::size_t sampleIndex) const {
    if (sampleMarks.empty() || sampleMarks[sampleIndex]) {
      return;
    }

    sampleMarks[sampleIndex] = 1;
    activeSampleIndices.push_back(sampleIndex);
  }

  std::uint64_t WhittedIntegrator::collectCurrentActiveSamples(
    const QueuedRayFrontier& current, std::vector<unsigned char>& activeSamples,
    std::vector<std::size_t>& activeSampleIndices) const {
    if (activeSamples.empty()) {
      return current.size();
    }

    clearActiveSampleMarks(activeSamples, activeSampleIndices);
    activeSampleIndices.clear();
    for (const auto& queued : current) {
      markActiveSample(activeSamples, activeSampleIndices, queued.sampleIndex);
    }

    return activeSampleIndices.size();
  }

  std::size_t WhittedIntegrator::partitionTraceableQueuedRays(QueuedRayFrontier& current) const {
    const auto firstTerminal =
      std::find_if_not(current.begin(), current.end(),
                       [this](const QueuedRay& queued) { return queuedRayShouldTrace(queued); });
    if (firstTerminal == current.end()) {
      return current.size();
    }

    const auto traceableEnd =
      std::stable_partition(firstTerminal, current.end(), [this](const QueuedRay& queued) {
        return queuedRayShouldTrace(queued);
      });
    return static_cast<std::size_t>(std::distance(current.begin(), traceableEnd));
  }

  void WhittedIntegrator::intersectQueuedRayScalar(
    const WavefrontIntersectionBackend& intersectionBackend, const Scene& scene,
    QueuedRayFrontier& current, std::size_t queuedIndex, ActiveQueuedHits& activeHits,
    std::vector<Colord>& result, BatchDepthMetrics& depthMetrics,
    IntegratorBatchMetrics* metrics) const {
    auto& queued = current[queuedIndex];
    if (isCancelled()) {
      result[queued.sampleIndex] += queued.weight * scene.background();
      return;
    }

    queued.state.recurseIn();

    if (queued.state.recursionDepth == m_maximumRecursionDepth) {
      recordQueuedRayTermination(
        scene, queued, result, "Raytracer: maximum recursion depth reached, returning background");
      return;
    }

    if (queued.state.throughput < RAYTRACER_THROUGHPUT_CUTOFF) {
      recordQueuedRayTermination(scene, queued, result,
                                 "Raytracer: throughput below cutoff, returning background");
      return;
    }

    if (depthMetrics.trackFrontierMetrics) {
      ++depthMetrics.frontierScalarRays;
    }
    WavefrontClosestHitResult hit;
    {
      WavefrontIntersectionQueryTiming intersectionTiming;
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      hit = intersectionBackend.intersectClosestResult(scene, queued.ray, queued.state,
                                                       &intersectionTiming);
      if (metrics) {
        metrics->recordClosestHitQuery(intersectionBackend, 1, intersectionTiming);
      }
    }
    if (isCancelled()) {
      result[queued.sampleIndex] += queued.weight * scene.background();
      queued.state.recurseOut();
      return;
    }

    if (!hit.hit()) {
      recordQueuedRayMiss(scene, queued, result, depthMetrics);
      return;
    }

    if (depthMetrics.trackFrontierMetrics) {
      ++depthMetrics.frontierRayHits;
    }
    activeHits.add(queuedIndex, hit.primitive, hit.material, hit.hitPoint);
  }

  void WhittedIntegrator::intersectQueuedRayPacket(
    const WavefrontIntersectionBackend& intersectionBackend, const Scene& scene,
    QueuedRayFrontier& current, std::size_t firstQueuedIndex, std::size_t laneCount,
    ActiveQueuedHits& activeHits, std::vector<Colord>& result, BatchDepthMetrics& depthMetrics,
    IntegratorBatchMetrics* metrics) const {
    const std::size_t activeLaneCount = std::min(laneCount, Ray4::lanes);
    std::array<Rayd, Ray4::lanes> rays{Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                       Rayd::undefined};
    std::optional<std::array<std::map<std::string, std::uint64_t>, Ray4::lanes>>
      packetFallbacksBefore;
    PrimitivePacketState4 states{};
    assert(laneCount <= Ray4::lanes);

    {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      if (depthMetrics.trackFrontierMetrics) {
        ++depthMetrics.frontierPacketChunks;
        ++depthMetrics.frontierRay4PacketChunks;
        depthMetrics.frontierPacketRays += activeLaneCount;
        packetFallbacksBefore.emplace();
      }
      for (std::size_t lane = 0; lane < Ray4::lanes && lane < laneCount; ++lane) {
        auto& queued = current[firstQueuedIndex + lane];
        queued.state.recurseIn();
        if (depthMetrics.trackFrontierMetrics) {
          (*packetFallbacksBefore)[lane] = queued.state.packetHitScalarFallbacksByReason;
        }
        rays[lane] = queued.ray;
        states[lane] = &queued.state;
      }
    }

    PrimitivePacketHit4 packetHits;
    {
      WavefrontIntersectionQueryTiming intersectionTiming;
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      packetHits =
        intersectionBackend.intersectPacketClosest(scene, Ray4(rays), states, &intersectionTiming);
      if (metrics) {
        metrics->recordClosestHitQuery(intersectionBackend, activeLaneCount, intersectionTiming);
      }
    }

    if (isCancelled()) {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      for (std::size_t lane = 0; lane != activeLaneCount; ++lane) {
        auto& queued = current[firstQueuedIndex + lane];
        result[queued.sampleIndex] += queued.weight * scene.background();
        queued.state.recurseOut();
      }
      return;
    }

    const auto packetNeedsRefinement = [&] {
      for (std::size_t lane = 0; lane != activeLaneCount; ++lane) {
        const Primitive* hitPrimitive = packetHits.primitive(lane);
        const auto hitMaterial = hitPrimitive ? hitPrimitive->material() : nullptr;
        if (hitMaterial && hitMaterial->requiresWhittedPacketHitRefinement() &&
            !packetHits.scalarFallback(lane)) {
          return true;
        }
      }
      return false;
    };
    if (!packetNeedsRefinement()) {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      for (std::size_t lane = 0; lane != activeLaneCount; ++lane) {
        auto& queued = current[firstQueuedIndex + lane];
        if (depthMetrics.trackFrontierMetrics) {
          depthMetrics.recordPacketScalarFallbacks(queued.state, (*packetFallbacksBefore)[lane]);
        }
        if (!packetHits.hit(lane)) {
          recordQueuedRayMiss(scene, queued, result, depthMetrics);
          continue;
        }

        if (depthMetrics.trackFrontierMetrics) {
          ++depthMetrics.frontierRayHits;
        }
        const Primitive* primitive = packetHits.primitive(lane);
        activeHits.add(firstQueuedIndex + lane, primitive, primitive->material(),
                       packetHits.hitPoint(lane));
      }
      return;
    }

    for (std::size_t lane = 0; lane != activeLaneCount; ++lane) {
      auto& queued = current[firstQueuedIndex + lane];
      if (depthMetrics.trackFrontierMetrics) {
        core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds
                                              : nullptr);
        depthMetrics.recordPacketScalarFallbacks(queued.state, (*packetFallbacksBefore)[lane]);
      }
      if (!packetHits.hit(lane)) {
        core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds
                                              : nullptr);
        recordQueuedRayMiss(scene, queued, result, depthMetrics);
        continue;
      }

      const Primitive* hitPrimitive = packetHits.primitive(lane);
      HitPoint hitPoint = packetHits.hitPoint(lane);
      const auto hitMaterial = hitPrimitive ? hitPrimitive->material() : nullptr;
      if (hitMaterial && hitMaterial->requiresWhittedPacketHitRefinement() &&
          !packetHits.scalarFallback(lane)) {
        if (depthMetrics.trackFrontierMetrics) {
          ++depthMetrics.frontierPacketRefinedRays;
          metrics->recordPacketHitRefinement(hitMaterial->whittedPacketHitRefinementLabel());
        }
        HitPointInterval refinedHitPoints;
        const Primitive* refinedPrimitive = nullptr;
        {
          WavefrontIntersectionQueryTiming intersectionTiming;
          core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
          refinedPrimitive = intersectionBackend.intersectClosest(
            scene, queued.ray, refinedHitPoints, queued.state, &intersectionTiming);
          if (metrics) {
            metrics->recordClosestHitQuery(intersectionBackend, 1, intersectionTiming);
          }
        }
        hitPrimitive = refinedPrimitive;
        if (refinedPrimitive) {
          hitPoint = refinedHitPoints.minWithPositiveDistance();
        }
      }
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      if (isCancelled()) {
        result[queued.sampleIndex] += queued.weight * scene.background();
        queued.state.recurseOut();
        continue;
      }
      if (!hitPrimitive) {
        recordQueuedRayMiss(scene, queued, result, depthMetrics);
        continue;
      }

      if (depthMetrics.trackFrontierMetrics) {
        ++depthMetrics.frontierRayHits;
      }
      activeHits.add(firstQueuedIndex + lane, hitPrimitive, hitPrimitive->material(), hitPoint);
    }
  }

  void WhittedIntegrator::intersectQueuedRayPacket8(
    const WavefrontIntersectionBackend& intersectionBackend, const Scene& scene,
    QueuedRayFrontier& current, std::size_t firstQueuedIndex, std::size_t laneCount,
    ActiveQueuedHits& activeHits, std::vector<Colord>& result, BatchDepthMetrics& depthMetrics,
    IntegratorBatchMetrics* metrics) const {
    const std::size_t activeLaneCount = std::min(laneCount, Ray8::lanes);
    std::array<Rayd, Ray8::lanes> rays{Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                       Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                       Rayd::undefined, Rayd::undefined};
    std::optional<std::array<std::map<std::string, std::uint64_t>, Ray8::lanes>>
      packetFallbacksBefore;
    PrimitivePacketState8 states{};
    assert(laneCount <= Ray8::lanes);

    {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      if (depthMetrics.trackFrontierMetrics) {
        ++depthMetrics.frontierPacketChunks;
        ++depthMetrics.frontierRay8PacketChunks;
        depthMetrics.frontierPacketRays += activeLaneCount;
        packetFallbacksBefore.emplace();
      }
      for (std::size_t lane = 0; lane < Ray8::lanes && lane < laneCount; ++lane) {
        auto& queued = current[firstQueuedIndex + lane];
        queued.state.recurseIn();
        if (depthMetrics.trackFrontierMetrics) {
          (*packetFallbacksBefore)[lane] = queued.state.packetHitScalarFallbacksByReason;
        }
        rays[lane] = queued.ray;
        states[lane] = &queued.state;
      }
    }

    PrimitivePacketHit8 packetHits;
    {
      WavefrontIntersectionQueryTiming intersectionTiming;
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      packetHits =
        intersectionBackend.intersectPacketClosest(scene, Ray8(rays), states, &intersectionTiming);
      if (metrics) {
        metrics->recordClosestHitQuery(intersectionBackend, activeLaneCount, intersectionTiming);
      }
    }

    if (isCancelled()) {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      for (std::size_t lane = 0; lane != activeLaneCount; ++lane) {
        auto& queued = current[firstQueuedIndex + lane];
        result[queued.sampleIndex] += queued.weight * scene.background();
        queued.state.recurseOut();
      }
      return;
    }

    const auto packetNeedsRefinement = [&] {
      for (std::size_t lane = 0; lane != activeLaneCount; ++lane) {
        const Primitive* hitPrimitive = packetHits.primitive(lane);
        const auto hitMaterial = hitPrimitive ? hitPrimitive->material() : nullptr;
        if (hitMaterial && hitMaterial->requiresWhittedPacketHitRefinement() &&
            !packetHits.scalarFallback(lane)) {
          return true;
        }
      }
      return false;
    };
    if (!packetNeedsRefinement()) {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      for (std::size_t lane = 0; lane != activeLaneCount; ++lane) {
        auto& queued = current[firstQueuedIndex + lane];
        if (depthMetrics.trackFrontierMetrics) {
          depthMetrics.recordPacketScalarFallbacks(queued.state, (*packetFallbacksBefore)[lane]);
        }
        if (!packetHits.hit(lane)) {
          recordQueuedRayMiss(scene, queued, result, depthMetrics);
          continue;
        }

        if (depthMetrics.trackFrontierMetrics) {
          ++depthMetrics.frontierRayHits;
        }
        const Primitive* primitive = packetHits.primitive(lane);
        activeHits.add(firstQueuedIndex + lane, primitive, primitive->material(),
                       packetHits.hitPoint(lane));
      }
      return;
    }

    for (std::size_t lane = 0; lane != activeLaneCount; ++lane) {
      auto& queued = current[firstQueuedIndex + lane];
      if (depthMetrics.trackFrontierMetrics) {
        core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds
                                              : nullptr);
        depthMetrics.recordPacketScalarFallbacks(queued.state, (*packetFallbacksBefore)[lane]);
      }
      if (!packetHits.hit(lane)) {
        core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds
                                              : nullptr);
        recordQueuedRayMiss(scene, queued, result, depthMetrics);
        continue;
      }

      const Primitive* hitPrimitive = packetHits.primitive(lane);
      HitPoint hitPoint = packetHits.hitPoint(lane);
      const auto hitMaterial = hitPrimitive ? hitPrimitive->material() : nullptr;
      if (hitMaterial && hitMaterial->requiresWhittedPacketHitRefinement() &&
          !packetHits.scalarFallback(lane)) {
        if (depthMetrics.trackFrontierMetrics) {
          ++depthMetrics.frontierPacketRefinedRays;
          metrics->recordPacketHitRefinement(hitMaterial->whittedPacketHitRefinementLabel());
        }
        HitPointInterval refinedHitPoints;
        const Primitive* refinedPrimitive = nullptr;
        {
          WavefrontIntersectionQueryTiming intersectionTiming;
          core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
          refinedPrimitive = intersectionBackend.intersectClosest(
            scene, queued.ray, refinedHitPoints, queued.state, &intersectionTiming);
          if (metrics) {
            metrics->recordClosestHitQuery(intersectionBackend, 1, intersectionTiming);
          }
        }
        hitPrimitive = refinedPrimitive;
        if (refinedPrimitive) {
          hitPoint = refinedHitPoints.minWithPositiveDistance();
        }
      }
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      if (isCancelled()) {
        result[queued.sampleIndex] += queued.weight * scene.background();
        queued.state.recurseOut();
        continue;
      }
      if (!hitPrimitive) {
        recordQueuedRayMiss(scene, queued, result, depthMetrics);
        continue;
      }

      if (depthMetrics.trackFrontierMetrics) {
        ++depthMetrics.frontierRayHits;
      }
      activeHits.add(firstQueuedIndex + lane, hitPrimitive, hitPrimitive->material(), hitPoint);
    }
  }

  void WhittedIntegrator::intersectQueuedRayBatch(
    const WavefrontIntersectionBackend& intersectionBackend, const Scene& scene,
    QueuedRayFrontier& current, std::size_t traceableCount, ActiveQueuedHits& activeHits,
    std::vector<Colord>& result, BatchDepthMetrics& depthMetrics,
    IntegratorBatchMetrics* metrics) const {
    ClosestHitQueuedRayFrontierBatch frontier(current, traceableCount, depthMetrics, metrics);
    frontier.intersect(scene, intersectionBackend, metrics);

    core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
    for (std::size_t queuedIndex = 0; queuedIndex != traceableCount; ++queuedIndex) {
      auto& queued = current[queuedIndex];
      if (isCancelled()) {
        result[queued.sampleIndex] += queued.weight * scene.background();
        queued.state.recurseOut();
        continue;
      }
      const WavefrontClosestHitResult* hit = frontier.hit(queuedIndex);
      if (!hit) {
        recordQueuedRayMiss(scene, queued, result, depthMetrics);
        continue;
      }

      if (depthMetrics.trackFrontierMetrics) {
        ++depthMetrics.frontierRayHits;
      }
      activeHits.add(queuedIndex, hit->primitive, hit->material, hit->hitPoint);
    }
  }

  void WhittedIntegrator::intersectActiveFrontier(
    const WavefrontIntersectionBackend& intersectionBackend, const Scene& scene,
    QueuedRayFrontier& current, ActiveQueuedHits& activeHits, std::vector<Colord>& result,
    BatchDepthMetrics& depthMetrics, IntegratorBatchMetrics* metrics) const {
    activeHits.clear();
    std::size_t traceableCount = 0;
    {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierPartitionWorkerSeconds : nullptr);
      traceableCount = partitionTraceableQueuedRays(current);
    }

    std::size_t queuedIndex = 0;
    if (!isCancelled() && intersectionBackend.prefersClosestHitBatch(traceableCount)) {
      intersectQueuedRayBatch(intersectionBackend, scene, current, traceableCount, activeHits,
                              result, depthMetrics, metrics);
      queuedIndex = traceableCount;
    }

    while (queuedIndex != traceableCount) {
      const bool canUsePacket8 = !isCancelled() && queuedIndex + Ray8::lanes <= traceableCount;
      if (canUsePacket8) {
        intersectQueuedRayPacket8(intersectionBackend, scene, current, queuedIndex, Ray8::lanes,
                                  activeHits, result, depthMetrics, metrics);
        queuedIndex += Ray8::lanes;
        continue;
      }

      const std::size_t remainingTraceable = traceableCount - queuedIndex;
      if (!isCancelled() && remainingTraceable > Ray4::lanes) {
        intersectQueuedRayPacket8(intersectionBackend, scene, current, queuedIndex,
                                  remainingTraceable, activeHits, result, depthMetrics, metrics);
        queuedIndex += remainingTraceable;
        continue;
      }

      const bool canUsePacket = !isCancelled() && queuedIndex + Ray4::lanes <= traceableCount;
      if (canUsePacket) {
        intersectQueuedRayPacket(intersectionBackend, scene, current, queuedIndex, Ray4::lanes,
                                 activeHits, result, depthMetrics, metrics);
        queuedIndex += Ray4::lanes;
        continue;
      }

      if (!isCancelled() && remainingTraceable > 1) {
        intersectQueuedRayPacket(intersectionBackend, scene, current, queuedIndex,
                                 remainingTraceable, activeHits, result, depthMetrics, metrics);
        queuedIndex += remainingTraceable;
        continue;
      }

      intersectQueuedRayScalar(intersectionBackend, scene, current, queuedIndex, activeHits, result,
                               depthMetrics, metrics);
      ++queuedIndex;
    }

    while (queuedIndex != current.size()) {
      intersectQueuedRayScalar(intersectionBackend, scene, current, queuedIndex, activeHits, result,
                               depthMetrics, metrics);
      ++queuedIndex;
    }
  }

  void WhittedIntegrator::queueOrResolveContinuation(
    const Scene& scene, const WhittedContinuation& continuation, const QueuedRay& parent,
    QueuedRayFrontier& next, std::vector<Colord>& result,
    std::vector<unsigned char>& nextActiveSamples, bool countNextActiveSamples,
    std::vector<std::size_t>& nextActiveSampleIndices) const {
    QueuedRay queued{
      parent.sampleIndex,
      continuation.ray,
      parent.weight * continuation.weight,
      continuationState(parent.state, parent.state.throughput * continuation.throughputScale),
    };

    if (queuedRayShouldTrace(queued)) {
      next.push(std::move(queued));
      if (countNextActiveSamples) {
        markActiveSample(nextActiveSamples, nextActiveSampleIndices, parent.sampleIndex);
      }
      return;
    }

    queued.state.recurseIn();
    if (queued.state.recursionDepth == m_maximumRecursionDepth) {
      recordQueuedRayTermination(
        scene, queued, result, "Raytracer: maximum recursion depth reached, returning background");
      return;
    }

    recordQueuedRayTermination(scene, queued, result,
                               "Raytracer: throughput below cutoff, returning background");
  }

  void WhittedIntegrator::shadeQueuedHit(const Scene& scene, const RayCaster& recursiveRayCaster,
                                         const QueuedHit& hit, QueuedRayFrontier& current,
                                         QueuedRayFrontier& next, std::vector<Colord>& result,
                                         std::vector<unsigned char>& nextActiveSamples,
                                         bool countNextActiveSamples,
                                         std::vector<std::size_t>& nextActiveSampleIndices,
                                         IntegratorBatchMetrics* metrics) const {
    auto& queued = current[hit.queuedIndex];
    ScopeExit recurseOut([&] { queued.state.recurseOut(); });
    core::util::ScopedTimer timer(metrics ? &metrics->shadingWorkerSeconds : nullptr);

    if (queued.state.recursionDepth == 1) {
      queued.state.hitPoint = hit.hitPoint;
    }

    const auto material = hit.material ? hit.material : hit.primitive->material();
    if (!material) {
      queued.state.recordEvent(nullptr, "Raytracer: no material found, returning black");
      return;
    }

    queued.state.recordEvent(nullptr, "Raytracer: shading material");
    if (!material->supportsWhittedContinuations()) {
      if (metrics) {
        metrics->usedScalarFallback = true;
        ++metrics->compatibilityShadeSamples;
      }
      result[queued.sampleIndex] +=
        queued.weight *
        material->shade(&recursiveRayCaster, scene, queued.ray, hit.hitPoint, queued.state);
      return;
    }

    const WhittedShadeResult shaded =
      material->shadeWhitted(&recursiveRayCaster, scene, queued.ray, hit.hitPoint, queued.state);
    result[queued.sampleIndex] += queued.weight * shaded.localRadiance;

    for (const auto& continuation : shaded.continuations) {
      queueOrResolveContinuation(scene, continuation, queued, next, result, nextActiveSamples,
                                 countNextActiveSamples, nextActiveSampleIndices);
    }
  }

  bool WhittedIntegrator::canUseBatchedLocalWhittedDirectLighting(const Material& material,
                                                                  const Rayd& ray,
                                                                  const HitPoint& hitPoint) const {
    const Vector3d out = -ray.direction();
    return material.supportsWhittedContinuations() && material.supportsBsdfSampling() &&
           material.deltaBsdfSamples(hitPoint, out).empty();
  }

  void WhittedIntegrator::DirectLightVisibilityBatch::collectLocalDirectLighting(
    const WhittedIntegrator& integrator, const Scene& scene, const ActiveQueuedHits& activeHits,
    QueuedRayFrontier& current, std::vector<Colord>& result, IntegratorBatchMetrics* metrics) {
    core::util::ScopedTimer timer(metrics ? &metrics->shadingWorkerSeconds : nullptr);
    for (std::size_t hitIndex = 0; hitIndex != activeHits.size(); ++hitIndex) {
      const QueuedHit& hit = activeHits[hitIndex];
      QueuedRay& queued = current[hit.queuedIndex];
      if (queued.state.recursionDepth == 1) {
        queued.state.hitPoint = hit.hitPoint;
      }

      const auto material = hit.material ? hit.material : hit.primitive->material();
      if (!material || !integrator.canUseBatchedLocalWhittedDirectLighting(*material, queued.ray,
                                                                           hit.hitPoint)) {
        continue;
      }

      markLocallyShaded(hitIndex);
      queued.state.recordEvent(nullptr, "Raytracer: shading material");
      result[queued.sampleIndex] +=
        queued.weight * material->ambientRadiance(scene, queued.ray, hit.hitPoint);

      const Vector3d out = -queued.ray.direction();
      for (const auto& light : scene.lights()) {
        const LightSample sample = light->sample(hit.hitPoint.point());
        const Vector3d in = sample.direction;
        const double normalDotIn = hit.hitPoint.normal() * in;
        Colord contribution = Colord::black();
        if (normalDotIn > 0.0) {
          contribution = material->evalBsdf(hit.hitPoint, out, in) * sample.radiance * normalDotIn;
        }
        add(hitIndex, sample, contribution, hit.hitPoint, queued.state);
      }
    }
  }

  void WhittedIntegrator::ActiveQueuedHits::shade(
    const WhittedIntegrator& integrator, const Scene& scene,
    const WavefrontIntersectionBackend& intersectionBackend, const RayCaster& recursiveRayCaster,
    int depth, QueuedRayFrontier& current, QueuedRayFrontier& next, std::vector<Colord>& result,
    std::vector<unsigned char>& nextActiveSamples, bool countNextActiveSamples,
    std::vector<std::size_t>& nextActiveSampleIndices, IntegratorBatchMetrics* metrics) const {
    if (empty()) {
      DirectLightVisibilityBatch emptyBatch(0, 0);
      emptyBatch.recordEmptyVisibility(depth, metrics);
      return;
    }

    DirectLightVisibilityBatch visibility(size(), size() * scene.lights().size());
    visibility.collectLocalDirectLighting(integrator, scene, *this, current, result, metrics);

    if (visibility.empty()) {
      visibility.recordEmptyVisibility(depth, metrics);
    } else {
      visibility.resolveOcclusion(scene, intersectionBackend, depth, metrics);
      core::util::ScopedTimer timer(metrics ? &metrics->shadingWorkerSeconds : nullptr);
      visibility.applyResolvedContributions(intersectionBackend, *this, current, result, depth,
                                            metrics);
    }

    for (std::size_t hitIndex = 0; hitIndex != size(); ++hitIndex) {
      const QueuedHit& hit = (*this)[hitIndex];
      if (visibility.locallyShaded(hitIndex)) {
        current[hit.queuedIndex].state.recurseOut();
        continue;
      }

      integrator.shadeQueuedHit(scene, recursiveRayCaster, hit, current, next, result,
                                nextActiveSamples, countNextActiveSamples, nextActiveSampleIndices,
                                metrics);
    }
  }

  std::vector<Colord> WhittedIntegrator::radianceBatch(
    const Scene& scene, const std::vector<IntegratorRaySample>& samples,
    const RayCaster& recursiveRayCaster, IntegratorBatchMetrics* metrics,
    const IntegratorBatchSettings& settings) const {
    if (metrics) {
      metrics->reset(/*scalarFallback=*/false);
    }
    const WavefrontIntersectionBackend& intersectionBackend =
      settings.resolvedIntersectionBackend();
    if (metrics) {
      metrics->recordIntersectionBackend(intersectionBackend);
      metrics->recordTracingScene(scene, intersectionBackend);
    }

    const bool trackRadianceDelta = metrics || settings.convergenceEnabled;
    const bool countCurrentActiveSamples = metrics || settings.convergenceEnabled;
    const bool countNextActiveSamples = settings.progressObserver || settings.convergenceEnabled;
    std::vector<Colord> result;
    std::vector<Colord> resultBeforeActiveSamples;
    std::vector<std::size_t> activeSampleIndices;
    std::vector<std::size_t> nextActiveSampleIndices;
    std::vector<unsigned char> activeSamples;
    std::vector<unsigned char> nextActiveSamples;
    activeSampleIndices.reserve(samples.size());
    nextActiveSampleIndices.reserve(samples.size());

    QueuedRayFrontier current;
    QueuedRayFrontier next;
    ActiveQueuedHits activeHits;
    {
      core::util::ScopedTimer timer(metrics ? &metrics->pathSetupWorkerSeconds : nullptr);
      result.resize(samples.size(), Colord::black());
      activeSamples.assign(countCurrentActiveSamples ? samples.size() : 0, 0);
      nextActiveSamples.assign(countNextActiveSamples ? samples.size() : 0, 0);
      current.reserve(samples.size());
      next.reserve(samples.size());
      activeHits.reserve(samples.size());
      for (std::size_t index = 0; index != samples.size(); ++index) {
        State state;
        state.timeSample = samples[index].timeSample;
        state.animationFrame = samples[index].animationFrame;
        state.animationTime = samples[index].animationTime;
        state.sampleStream = samples[index].sampleStream();
        current.push(QueuedRay{index, samples[index].ray, Colord::white(), std::move(state)});
      }
    }

    for (int depth = 0; depth != m_maximumRecursionDepth && !current.empty(); ++depth) {
      const std::uint64_t currentActiveSamples =
        collectCurrentActiveSamples(current, activeSamples, activeSampleIndices);
      if (metrics) {
        metrics->recordActiveDepth(currentActiveSamples);
        current.recordActiveHostPathStateBytes(metrics);
      }
      if (trackRadianceDelta) {
        resultBeforeActiveSamples.clear();
        resultBeforeActiveSamples.reserve(activeSampleIndices.size());
        for (const std::size_t sampleIndex : activeSampleIndices) {
          resultBeforeActiveSamples.push_back(result[sampleIndex]);
        }
      }

      next.prepareForNextDepth(current.size());
      if (countNextActiveSamples) {
        clearActiveSampleMarks(nextActiveSamples, nextActiveSampleIndices);
        nextActiveSampleIndices.clear();
      }

      BatchDepthMetrics depthMetrics;
      depthMetrics.trackFrontierMetrics = metrics != nullptr;
      intersectActiveFrontier(intersectionBackend, scene, current, activeHits, result, depthMetrics,
                              metrics);
      if (metrics) {
        metrics->recordActiveHitHostBytes(activeHits.hostBytes());
        depthMetrics.publish(metrics);
      }

      activeHits.shade(*this, scene, intersectionBackend, recursiveRayCaster, depth, current, next,
                       result, nextActiveSamples, countNextActiveSamples, nextActiveSampleIndices,
                       metrics);

      double depthDeltaSquaredSum = 0.0;
      double depthMaxDelta = 0.0;
      if (trackRadianceDelta) {
        for (std::size_t activeIndex = 0; activeIndex != activeSampleIndices.size();
             ++activeIndex) {
          const std::size_t sampleIndex = activeSampleIndices[activeIndex];
          const double deltaSquared =
            radianceDeltaSquared(resultBeforeActiveSamples[activeIndex], result[sampleIndex]);
          depthDeltaSquaredSum += deltaSquared;
          if (metrics) {
            depthMaxDelta = std::max(depthMaxDelta, std::sqrt(deltaSquared));
          }
        }
      }

      if (metrics) {
        metrics->recordRadianceDeltaDepth(depthDeltaSquaredSum, depthMaxDelta);
      }

      const std::uint64_t nextActiveSampleCount =
        next.retainedActiveSampleCount(countNextActiveSamples, nextActiveSampleIndices);
      next.recordCompletedDepth(nextActiveSampleCount, metrics);
      IntegratorBatchFeedback feedback;
      if (settings.progressObserver) {
        core::util::ScopedTimer timer(metrics ? &metrics->progressSnapshotWorkerSeconds : nullptr);
        feedback = settings.progressObserver->depthCompleted(static_cast<std::uint64_t>(depth + 1),
                                                             result, nextActiveSampleCount);
      }

      if (settings.convergenceEnabled && !samples.empty()) {
        core::util::ScopedTimer timer(metrics ? &metrics->convergenceTestWorkerSeconds : nullptr);
        const double activeFraction =
          static_cast<double>(nextActiveSampleCount) / static_cast<double>(samples.size());
        const double rawRadianceDeltaRms =
          currentActiveSamples == 0
            ? 0.0
            : std::sqrt(depthDeltaSquaredSum / static_cast<double>(currentActiveSamples));
        const double radianceDeltaRms =
          feedback.convergenceRadianceDeltaRms.value_or(rawRadianceDeltaRms);
        if (metrics && feedback.convergenceRadianceDeltaRms) {
          ++metrics->observerConvergenceFeedbackDepths;
        }
        if (activeFraction <= settings.activeSampleFractionThreshold &&
            radianceDeltaRms <= settings.radianceDeltaRmsThreshold) {
          if (metrics) {
            metrics->stoppedByConvergence = true;
            metrics->stoppedAfterDepth = metrics->activeSamplesPerDepth.size();
          }
          break;
        }
      }

      current.advanceTo(next);
    }

    return result;
  }

  void WhittedIntegrator::setMaximumRecursionDepth(int depth) {
    m_maximumRecursionDepth = depth;
  }

  int WhittedIntegrator::maximumRecursionDepth() const {
    return m_maximumRecursionDepth;
  }

  void WhittedIntegrator::setCancellationCallback(CancellationCallback callback) {
    m_cancellationCallback = std::move(callback);
  }

  bool WhittedIntegrator::isCancelled() const {
    return m_cancellationCallback && m_cancellationCallback();
  }

  State WhittedIntegrator::continuationState(const State& parent, double throughput) const {
    State result;
    result.recursionDepth = parent.recursionDepth;
    result.maxRecursionDepth = parent.maxRecursionDepth;
    result.timeSample = parent.timeSample;
    result.animationFrame = parent.animationFrame;
    result.animationTime = parent.animationTime;
    result.throughput = throughput;
    result.sampleStream = parent.sampleStream;
    return result;
  }
}
