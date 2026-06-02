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
    std::uint64_t frontierScalarRays{0};
    std::uint64_t frontierPacketScalarFallbackRays{0};
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
    ++depthMetrics.frontierRayMisses;
    queued.state.recordEvent(nullptr, "Raytracer: Nothing hit, returning background color");
    result[queued.sampleIndex] += queued.weight * scene.background();
    queued.state.recurseOut();
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

    ++depthMetrics.frontierScalarRays;
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

    ++depthMetrics.frontierRayHits;
    activeHits.push_back(QueuedHit{queuedIndex, primitive, hitPoints.minWithPositiveDistance()});
  }

  void WhittedIntegrator::intersectQueuedRayPacket(
    const Scene& scene, std::vector<QueuedRay>& current, std::size_t firstQueuedIndex,
    std::vector<QueuedHit>& activeHits, std::vector<Colord>& result,
    BatchDepthMetrics& depthMetrics, IntegratorBatchMetrics* metrics) const {
    std::array<Rayd, Ray4::lanes> rays{Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                       Rayd::undefined};
    std::array<std::uint64_t, Ray4::lanes> packetFallbacksBefore{};
    PrimitivePacketState4 states{};

    {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      ++depthMetrics.frontierPacketChunks;
      for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
        auto& queued = current[firstQueuedIndex + lane];
        queued.state.recurseIn();
        packetFallbacksBefore[lane] = queued.state.packetHitScalarFallbacks;
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
        auto& queued = current[firstQueuedIndex + lane];
        result[queued.sampleIndex] += queued.weight * scene.background();
        queued.state.recurseOut();
      }
      return;
    }

    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      auto& queued = current[firstQueuedIndex + lane];
      {
        core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds
                                              : nullptr);
        depthMetrics.frontierPacketScalarFallbackRays +=
          queued.state.packetHitScalarFallbacks - packetFallbacksBefore[lane];
      }
      if (!packetHits.hit(lane)) {
        core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds
                                              : nullptr);
        recordQueuedRayMiss(scene, queued, result, depthMetrics);
        continue;
      }

      HitPointInterval refinedHitPoints;
      const Primitive* refinedPrimitive = nullptr;
      {
        core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
        refinedPrimitive = scene.intersect(queued.ray, refinedHitPoints, queued.state);
      }
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      if (isCancelled()) {
        result[queued.sampleIndex] += queued.weight * scene.background();
        queued.state.recurseOut();
        continue;
      }
      if (!refinedPrimitive) {
        recordQueuedRayMiss(scene, queued, result, depthMetrics);
        continue;
      }

      ++depthMetrics.frontierRayHits;
      activeHits.push_back(QueuedHit{firstQueuedIndex + lane, refinedPrimitive,
                                     refinedHitPoints.minWithPositiveDistance()});
    }
  }

  void WhittedIntegrator::intersectActiveFrontier(const Scene& scene,
                                                  std::vector<QueuedRay>& current,
                                                  std::vector<QueuedHit>& activeHits,
                                                  std::vector<Colord>& result,
                                                  BatchDepthMetrics& depthMetrics,
                                                  IntegratorBatchMetrics* metrics) const {
    activeHits.clear();
    std::size_t queuedIndex = 0;
    while (queuedIndex != current.size()) {
      const bool canUsePacket =
        !isCancelled() && queuedIndex + Ray4::lanes <= current.size() &&
        std::all_of(current.begin() + static_cast<std::ptrdiff_t>(queuedIndex),
                    current.begin() + static_cast<std::ptrdiff_t>(queuedIndex + Ray4::lanes),
                    [this](const QueuedRay& queued) { return queuedRayShouldTrace(queued); });
      if (canUsePacket) {
        intersectQueuedRayPacket(scene, current, queuedIndex, activeHits, result, depthMetrics,
                                 metrics);
        queuedIndex += Ray4::lanes;
        continue;
      }

      intersectQueuedRayScalar(scene, current, queuedIndex, activeHits, result, depthMetrics,
                               metrics);
      ++queuedIndex;
    }
  }

  void WhittedIntegrator::shadeQueuedHit(const Scene& scene, const RayCaster& recursiveRayCaster,
                                         const QueuedHit& hit, std::vector<QueuedRay>& current,
                                         std::vector<QueuedRay>& next, std::vector<Colord>& result,
                                         std::vector<unsigned char>& nextActiveSamples,
                                         bool countNextActiveSamples,
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
      next.push_back(QueuedRay{
        queued.sampleIndex,
        continuation.ray,
        queued.weight * continuation.weight,
        continuationState(queued.state, queued.state.throughput * continuation.throughputScale),
      });
      if (countNextActiveSamples) {
        nextActiveSamples[queued.sampleIndex] = 1;
      }
    }
  }

  std::vector<Colord> WhittedIntegrator::radianceBatch(
    const Scene& scene, const std::vector<IntegratorRaySample>& samples,
    const RayCaster& recursiveRayCaster, IntegratorBatchMetrics* metrics,
    const IntegratorBatchSettings& settings) const {
    if (metrics) {
      metrics->reset(/*scalarFallback=*/false);
    }

    std::vector<Colord> result(samples.size(), Colord::black());
    const bool trackRadianceDelta = metrics || settings.convergenceEnabled;
    const bool countCurrentActiveSamples = metrics || settings.convergenceEnabled;
    const bool countNextActiveSamples = settings.progressObserver || settings.convergenceEnabled;
    std::vector<Colord> resultBeforeActiveSamples;
    std::vector<std::size_t> activeSampleIndices;
    std::vector<unsigned char> activeSamples(countCurrentActiveSamples ? samples.size() : 0, 0);
    std::vector<unsigned char> nextActiveSamples(countNextActiveSamples ? samples.size() : 0, 0);
    activeSampleIndices.reserve(samples.size());

    std::vector<QueuedRay> current;
    current.reserve(samples.size());
    std::vector<QueuedHit> activeHits;
    activeHits.reserve(samples.size());
    for (std::size_t index = 0; index != samples.size(); ++index) {
      State state;
      state.timeSample = samples[index].timeSample;
      state.sampleStream = samples[index].sampleStream();
      current.push_back(QueuedRay{index, samples[index].ray, Colord::white(), std::move(state)});
    }

    for (int depth = 0; depth != m_maximumRecursionDepth && !current.empty(); ++depth) {
      std::uint64_t currentActiveSamples = current.size();
      if (countCurrentActiveSamples) {
        std::fill(activeSamples.begin(), activeSamples.end(), 0);
        for (const auto& queued : current) {
          activeSamples[queued.sampleIndex] = 1;
        }
        activeSampleIndices.clear();
        for (std::size_t sampleIndex = 0; sampleIndex != activeSamples.size(); ++sampleIndex) {
          if (activeSamples[sampleIndex]) {
            activeSampleIndices.push_back(sampleIndex);
          }
        }
        currentActiveSamples = activeSampleIndices.size();
      }
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

      std::vector<QueuedRay> next;
      next.reserve(current.size());
      if (countNextActiveSamples) {
        std::fill(nextActiveSamples.begin(), nextActiveSamples.end(), 0);
      }

      BatchDepthMetrics depthMetrics;
      intersectActiveFrontier(scene, current, activeHits, result, depthMetrics, metrics);
      if (metrics) {
        metrics->recordFrontierIntersections(depthMetrics.frontierRayHits,
                                             depthMetrics.frontierRayMisses);
        metrics->recordFrontierTraversal(depthMetrics.frontierPacketChunks,
                                         depthMetrics.frontierScalarRays,
                                         depthMetrics.frontierPacketScalarFallbackRays);
      }

      for (const auto& hit : activeHits) {
        shadeQueuedHit(scene, recursiveRayCaster, hit, current, next, result, nextActiveSamples,
                       countNextActiveSamples, metrics);
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
        countNextActiveSamples ? static_cast<std::uint64_t>(std::count(nextActiveSamples.begin(),
                                                                       nextActiveSamples.end(), 1))
                               : next.size();
      if (settings.progressObserver) {
        settings.progressObserver->depthCompleted(static_cast<std::uint64_t>(depth + 1), result,
                                                  nextActiveSampleCount);
      }

      if (settings.convergenceEnabled && !samples.empty()) {
        const double activeFraction =
          static_cast<double>(nextActiveSampleCount) / static_cast<double>(samples.size());
        const double radianceDeltaRms =
          currentActiveSamples == 0
            ? 0.0
            : std::sqrt(depthDeltaSquaredSum / static_cast<double>(currentActiveSamples));
        if (activeFraction <= settings.activeSampleFractionThreshold &&
            radianceDeltaRms <= settings.radianceDeltaRmsThreshold) {
          if (metrics) {
            metrics->stoppedByConvergence = true;
            metrics->stoppedAfterDepth = metrics->activeSamplesPerDepth.size();
          }
          break;
        }
      }

      current = std::move(next);
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
