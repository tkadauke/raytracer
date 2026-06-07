#include "render/WhittedIntegrator.h"

#include "core/ScopeExit.h"
#include "core/math/Constants.h"
#include "core/math/HitPoint.h"
#include "core/math/HitPointInterval.h"
#include "core/util/ScopedTimer.h"
#include "render/RayCaster.h"
#include "render/State.h"
#include "render/materials/Material.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"

#include <algorithm>
#include <array>
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

  struct WhittedIntegrator::QueuedRay {
    std::size_t sampleIndex{0};
    Rayd ray;
    Colord weight{Colord::white()};
    State state;
  };

  struct WhittedIntegrator::QueuedHit {
    std::size_t queuedIndex{0};
    const Primitive* primitive{nullptr};
    HitPoint hitPoint;
  };

  struct WhittedIntegrator::BatchDepthMetrics {
    std::uint64_t frontierRayHits{0};
    std::uint64_t frontierRayMisses{0};
    std::uint64_t frontierPacketChunks{0};
    std::uint64_t frontierPacketRays{0};
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
    const std::vector<QueuedRay>& current, std::vector<unsigned char>& activeSamples,
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

  std::size_t
  WhittedIntegrator::partitionTraceableQueuedRays(std::vector<QueuedRay>& current) const {
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
    const Scene& scene, std::vector<QueuedRay>& current, std::size_t queuedIndex,
    std::vector<QueuedHit>& activeHits, std::vector<Colord>& result,
    BatchDepthMetrics& depthMetrics, IntegratorBatchMetrics* metrics) const {
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
    HitPointInterval hitPoints;
    const Primitive* primitive = nullptr;
    {
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      primitive = scene.intersect(queued.ray, hitPoints, queued.state);
    }
    if (isCancelled()) {
      result[queued.sampleIndex] += queued.weight * scene.background();
      queued.state.recurseOut();
      return;
    }

    if (!primitive) {
      recordQueuedRayMiss(scene, queued, result, depthMetrics);
      return;
    }

    if (depthMetrics.trackFrontierMetrics) {
      ++depthMetrics.frontierRayHits;
    }
    activeHits.push_back(QueuedHit{queuedIndex, primitive, hitPoints.minWithPositiveDistance()});
  }

  void WhittedIntegrator::intersectQueuedRayPacket(
    const Scene& scene, std::vector<QueuedRay>& current, std::size_t firstQueuedIndex,
    std::size_t laneCount, std::vector<QueuedHit>& activeHits, std::vector<Colord>& result,
    BatchDepthMetrics& depthMetrics, IntegratorBatchMetrics* metrics) const {
    if (laneCount > Ray4::lanes) {
      throw std::logic_error("Ray4 queued-ray packet lane count exceeds packet width");
    }
    const std::size_t packetLaneCount = std::min(laneCount, Ray4::lanes);
    std::array<Rayd, Ray4::lanes> rays{Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                       Rayd::undefined};
    std::optional<std::array<std::map<std::string, std::uint64_t>, Ray4::lanes>>
      packetFallbacksBefore;
    PrimitivePacketState4 states{};

    {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      if (depthMetrics.trackFrontierMetrics) {
        ++depthMetrics.frontierPacketChunks;
        ++depthMetrics.frontierRay4PacketChunks;
        depthMetrics.frontierPacketRays += packetLaneCount;
        packetFallbacksBefore.emplace();
      }
      for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
        if (lane == packetLaneCount)
          break;
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
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      packetHits = scene.intersectPacketHits(Ray4(rays), states);
    }

    if (isCancelled()) {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
        if (lane == packetLaneCount)
          break;
        auto& queued = current[firstQueuedIndex + lane];
        result[queued.sampleIndex] += queued.weight * scene.background();
        queued.state.recurseOut();
      }
      return;
    }

    const auto packetNeedsRefinement = [&] {
      for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
        if (lane == packetLaneCount)
          break;
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
      for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
        if (lane == packetLaneCount)
          break;
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
        activeHits.push_back(QueuedHit{firstQueuedIndex + lane, packetHits.primitive(lane),
                                       packetHits.hitPoint(lane)});
      }
      return;
    }

    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      if (lane == packetLaneCount)
        break;
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
          core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
          refinedPrimitive = scene.intersect(queued.ray, refinedHitPoints, queued.state);
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
      activeHits.push_back(QueuedHit{firstQueuedIndex + lane, hitPrimitive, hitPoint});
    }
  }

  void WhittedIntegrator::intersectQueuedRayPacket8(
    const Scene& scene, std::vector<QueuedRay>& current, std::size_t firstQueuedIndex,
    std::size_t laneCount, std::vector<QueuedHit>& activeHits, std::vector<Colord>& result,
    BatchDepthMetrics& depthMetrics, IntegratorBatchMetrics* metrics) const {
    if (laneCount > Ray8::lanes) {
      throw std::logic_error("Ray8 queued-ray packet lane count exceeds packet width");
    }
    const std::size_t packetLaneCount = std::min(laneCount, Ray8::lanes);
    std::array<Rayd, Ray8::lanes> rays{Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                       Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                       Rayd::undefined, Rayd::undefined};
    std::optional<std::array<std::map<std::string, std::uint64_t>, Ray8::lanes>>
      packetFallbacksBefore;
    PrimitivePacketState8 states{};

    {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      if (depthMetrics.trackFrontierMetrics) {
        ++depthMetrics.frontierPacketChunks;
        ++depthMetrics.frontierRay8PacketChunks;
        depthMetrics.frontierPacketRays += packetLaneCount;
        packetFallbacksBefore.emplace();
      }
      for (std::size_t lane = 0; lane != Ray8::lanes; ++lane) {
        if (lane == packetLaneCount)
          break;
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
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      packetHits = scene.intersectPacketHits(Ray8(rays), states);
    }

    if (isCancelled()) {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      for (std::size_t lane = 0; lane != Ray8::lanes; ++lane) {
        if (lane == packetLaneCount)
          break;
        auto& queued = current[firstQueuedIndex + lane];
        result[queued.sampleIndex] += queued.weight * scene.background();
        queued.state.recurseOut();
      }
      return;
    }

    const auto packetNeedsRefinement = [&] {
      for (std::size_t lane = 0; lane != Ray8::lanes; ++lane) {
        if (lane == packetLaneCount)
          break;
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
      for (std::size_t lane = 0; lane != Ray8::lanes; ++lane) {
        if (lane == packetLaneCount)
          break;
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
        activeHits.push_back(QueuedHit{firstQueuedIndex + lane, packetHits.primitive(lane),
                                       packetHits.hitPoint(lane)});
      }
      return;
    }

    for (std::size_t lane = 0; lane != Ray8::lanes; ++lane) {
      if (lane == packetLaneCount)
        break;
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
          core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
          refinedPrimitive = scene.intersect(queued.ray, refinedHitPoints, queued.state);
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
      activeHits.push_back(QueuedHit{firstQueuedIndex + lane, hitPrimitive, hitPoint});
    }
  }

  void WhittedIntegrator::intersectActiveFrontier(const Scene& scene,
                                                  std::vector<QueuedRay>& current,
                                                  std::vector<QueuedHit>& activeHits,
                                                  std::vector<Colord>& result,
                                                  BatchDepthMetrics& depthMetrics,
                                                  IntegratorBatchMetrics* metrics) const {
    activeHits.clear();
    std::size_t traceableCount = 0;
    {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierPartitionWorkerSeconds : nullptr);
      traceableCount = partitionTraceableQueuedRays(current);
    }

    std::size_t queuedIndex = 0;
    while (queuedIndex != traceableCount) {
      const bool canUsePacket8 = !isCancelled() && queuedIndex + Ray8::lanes <= traceableCount;
      if (canUsePacket8) {
        intersectQueuedRayPacket8(scene, current, queuedIndex, Ray8::lanes, activeHits, result,
                                  depthMetrics, metrics);
        queuedIndex += Ray8::lanes;
        continue;
      }

      const std::size_t remainingTraceable = traceableCount - queuedIndex;
      if (!isCancelled() && remainingTraceable > Ray4::lanes) {
        intersectQueuedRayPacket8(scene, current, queuedIndex, remainingTraceable, activeHits,
                                  result, depthMetrics, metrics);
        queuedIndex += remainingTraceable;
        continue;
      }

      const bool canUsePacket = !isCancelled() && queuedIndex + Ray4::lanes <= traceableCount;
      if (canUsePacket) {
        intersectQueuedRayPacket(scene, current, queuedIndex, Ray4::lanes, activeHits, result,
                                 depthMetrics, metrics);
        queuedIndex += Ray4::lanes;
        continue;
      }

      if (!isCancelled() && remainingTraceable > 1) {
        intersectQueuedRayPacket(scene, current, queuedIndex, remainingTraceable, activeHits,
                                 result, depthMetrics, metrics);
        queuedIndex += remainingTraceable;
        continue;
      }

      intersectQueuedRayScalar(scene, current, queuedIndex, activeHits, result, depthMetrics,
                               metrics);
      ++queuedIndex;
    }

    while (queuedIndex != current.size()) {
      intersectQueuedRayScalar(scene, current, queuedIndex, activeHits, result, depthMetrics,
                               metrics);
      ++queuedIndex;
    }
  }

  void WhittedIntegrator::prepareContinuationQueue(std::vector<QueuedRay>& next,
                                                   std::size_t currentQueueSize) const {
    next.clear();
    if (next.capacity() < currentQueueSize) {
      next.reserve(currentQueueSize);
    }
  }

  void WhittedIntegrator::queueOrResolveContinuation(
    const Scene& scene, const WhittedContinuation& continuation, const QueuedRay& parent,
    std::vector<QueuedRay>& next, std::vector<Colord>& result,
    std::vector<unsigned char>& nextActiveSamples, bool countNextActiveSamples,
    std::vector<std::size_t>& nextActiveSampleIndices) const {
    QueuedRay queued{
      parent.sampleIndex,
      continuation.ray,
      parent.weight * continuation.weight,
      continuationState(parent.state, parent.state.throughput * continuation.throughputScale),
    };

    if (queuedRayShouldTrace(queued)) {
      next.push_back(std::move(queued));
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
                                         const QueuedHit& hit, std::vector<QueuedRay>& current,
                                         std::vector<QueuedRay>& next, std::vector<Colord>& result,
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

    const auto material = hit.primitive->material();
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

  std::vector<Colord> WhittedIntegrator::radianceBatch(
    const Scene& scene, const std::vector<IntegratorRaySample>& samples,
    const RayCaster& recursiveRayCaster, IntegratorBatchMetrics* metrics,
    const IntegratorBatchSettings& settings) const {
    if (metrics) {
      metrics->reset(/*scalarFallback=*/false);
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

    std::vector<QueuedRay> current;
    std::vector<QueuedRay> next;
    std::vector<QueuedHit> activeHits;
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
        state.sampleStream = samples[index].sampleStream();
        current.push_back(QueuedRay{index, samples[index].ray, Colord::white(), std::move(state)});
      }
    }

    for (int depth = 0; depth != m_maximumRecursionDepth && !current.empty(); ++depth) {
      const std::uint64_t currentActiveSamples =
        collectCurrentActiveSamples(current, activeSamples, activeSampleIndices);
      if (metrics) {
        metrics->recordActiveDepth(currentActiveSamples);
      }
      if (trackRadianceDelta) {
        resultBeforeActiveSamples.clear();
        resultBeforeActiveSamples.reserve(activeSampleIndices.size());
        for (const std::size_t sampleIndex : activeSampleIndices) {
          resultBeforeActiveSamples.push_back(result[sampleIndex]);
        }
      }

      prepareContinuationQueue(next, current.size());
      if (countNextActiveSamples) {
        clearActiveSampleMarks(nextActiveSamples, nextActiveSampleIndices);
        nextActiveSampleIndices.clear();
      }

      BatchDepthMetrics depthMetrics;
      depthMetrics.trackFrontierMetrics = metrics != nullptr;
      intersectActiveFrontier(scene, current, activeHits, result, depthMetrics, metrics);
      if (metrics) {
        metrics->recordFrontierIntersections(depthMetrics.frontierRayHits,
                                             depthMetrics.frontierRayMisses);
        metrics->recordFrontierTraversal(
          depthMetrics.frontierPacketChunks, depthMetrics.frontierPacketRays,
          depthMetrics.frontierRay4PacketChunks, depthMetrics.frontierRay8PacketChunks,
          depthMetrics.frontierScalarRays, depthMetrics.frontierPacketScalarFallbackRays,
          depthMetrics.frontierPacketRefinedRays);
        metrics->recordPacketScalarFallbacksByReason(
          depthMetrics.frontierPacketScalarFallbackRaysByReason);
      }

      for (const auto& hit : activeHits) {
        shadeQueuedHit(scene, recursiveRayCaster, hit, current, next, result, nextActiveSamples,
                       countNextActiveSamples, nextActiveSampleIndices, metrics);
      }

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
        countNextActiveSamples ? nextActiveSampleIndices.size() : next.size();
      if (metrics) {
        metrics->recordRetainedActiveDepth(nextActiveSampleCount);
      }
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

      current.swap(next);
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
    result.throughput = throughput;
    result.sampleStream = parent.sampleStream;
    return result;
  }
}
