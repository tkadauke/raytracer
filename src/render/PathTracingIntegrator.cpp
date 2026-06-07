#include "render/PathTracingIntegrator.h"

#include "core/math/HitPoint.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/util/ScopedTimer.h"
#include "render/MIS.h"
#include "render/RayCaster.h"
#include "render/State.h"
#include "render/lights/Light.h"
#include "render/materials/Material.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"
#include "render/samplers/SampleStream.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace render {
  struct PathTracingIntegrator::BatchPath {
    BatchPath(const IntegratorRaySample& sample, Colord& accumulated)
        : ray(sample.ray),
          m_accumulated(&accumulated) {
      this->accumulated() = Colord::black();
      state.timeSample = sample.timeSample;
      state.sampleStream = sample.sampleStream();
    }

    BatchPath(Rayd nextRay, Colord nextThroughput, bool nextBackgroundVisible, State nextState,
              Colord& accumulated)
        : ray(std::move(nextRay)),
          throughput(nextThroughput),
          backgroundVisible(nextBackgroundVisible),
          state(std::move(nextState)),
          m_accumulated(&accumulated) {
    }

    Colord& accumulated() {
      return *m_accumulated;
    }

    const Colord& accumulated() const {
      return *m_accumulated;
    }

    Rayd ray;
    Colord throughput{Colord::white()};
    bool backgroundVisible{true};
    State state;

  private:
    Colord* m_accumulated;
  };

  struct PathTracingIntegrator::ScalarPath {
    ScalarPath(const Rayd& primaryRay, State& primaryState)
        : ray(primaryRay),
          state(&primaryState) {
    }

    ScalarPath(Rayd nextRay, Colord nextThroughput, bool nextBackgroundVisible, State nextState)
        : ray(std::move(nextRay)),
          throughput(nextThroughput),
          backgroundVisible(nextBackgroundVisible),
          ownedState(std::make_unique<State>(std::move(nextState))),
          state(ownedState.get()) {
    }

    ScalarPath(ScalarPath&&) noexcept = default;
    ScalarPath& operator=(ScalarPath&&) noexcept = default;

    State& pathState() {
      return *state;
    }

    Rayd ray;
    Colord throughput{Colord::white()};
    bool backgroundVisible{true};
    std::unique_ptr<State> ownedState;
    State* state{nullptr};
  };

  struct PathTracingIntegrator::BatchHit {
    std::size_t pathIndex{0};
    const Primitive* primitive{nullptr};
    HitPoint hitPoint;
  };

  struct PathTracingIntegrator::BatchDepthMetrics {
    bool trackRadianceDelta{false};
    std::uint64_t frontierRayHits{0};
    std::uint64_t frontierRayMisses{0};
    std::uint64_t frontierPacketChunks{0};
    std::uint64_t frontierPacketRays{0};
    std::uint64_t frontierRay4PacketChunks{0};
    std::uint64_t frontierRay8PacketChunks{0};
    std::uint64_t frontierScalarRays{0};
    std::uint64_t frontierPacketScalarFallbackRays{0};
    std::map<std::string, std::uint64_t> frontierPacketScalarFallbackRaysByReason;
    double depthDeltaSquaredSum{0.0};
    double depthMaxDelta{0.0};
    IntegratorBatchMetrics* metrics{nullptr};

    bool trackFrontierMetrics() const {
      return metrics != nullptr;
    }

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

  PathTracingIntegrator::PathTracingIntegrator() = default;

  std::unique_ptr<Integrator> PathTracingIntegrator::clone() const {
    auto result = std::make_unique<PathTracingIntegrator>();
    result->setMaximumRecursionDepth(m_maximumRecursionDepth);
    result->setRussianRouletteDepth(m_russianRouletteDepth);
    result->setCancellationCallback(m_cancellationCallback);
    return result;
  }

  const char* PathTracingIntegrator::diagnosticName() const {
    return "pathtracer";
  }

  const char* PathTracingIntegrator::batchExecutionMode() const {
    return "depth_major_paths";
  }

  void PathTracingIntegrator::setCancellationCallback(CancellationCallback callback) {
    m_cancellationCallback = std::move(callback);
  }

  bool PathTracingIntegrator::isCancelled() const {
    return m_cancellationCallback && m_cancellationCallback();
  }

  State PathTracingIntegrator::clonePathState(const State& state) const {
    State result;
    result.traceEvents = state.traceEvents;
    result.numRays = state.numRays;
    result.recursionDepth = state.recursionDepth;
    result.maxRecursionDepth = state.maxRecursionDepth;
    result.intersectionHits = state.intersectionHits;
    result.intersectionMisses = state.intersectionMisses;
    result.shadowIntersectionHits = state.shadowIntersectionHits;
    result.shadowIntersectionMisses = state.shadowIntersectionMisses;
    result.packetHitScalarFallbacks = state.packetHitScalarFallbacks;
    result.packetHitScalarFallbacksByReason = state.packetHitScalarFallbacksByReason;
    result.hitPoint = state.hitPoint;
    result.timeSample = state.timeSample;
    result.throughput = state.throughput;
    result.sampleStream = state.sampleStream;
    if (state.events) {
      result.events = std::make_unique<std::list<std::string>>(*state.events);
    }
    return result;
  }

  Colord PathTracingIntegrator::missRadiance(const Scene& scene, bool backgroundVisible) const {
    return backgroundVisible ? scene.background() : scene.environmentRadiance();
  }

  bool PathTracingIntegrator::canContinueWithSample(const MaterialBsdfSample& sample,
                                                    const HitPoint& hitPoint) const {
    if (sample.pdf <= 0.0 || sample.value == Colord::black()) {
      return false;
    }

    const double normalDotWo = hitPoint.normal() * sample.direction;
    return sample.isDelta || normalDotWo > 0.0;
  }

  Colord PathTracingIntegrator::continuedThroughput(const Colord& throughput,
                                                    const MaterialBsdfSample& sample,
                                                    const HitPoint& hitPoint) const {
    if (sample.isDelta) {
      return throughput * sample.value;
    }

    const double normalDotWo = hitPoint.normal() * sample.direction;
    return throughput * (sample.value * (normalDotWo / sample.pdf));
  }

  bool PathTracingIntegrator::survivesRussianRoulette(Colord& throughput, State& state,
                                                      int bounce) const {
    if (bounce < m_russianRouletteDepth) {
      return true;
    }

    const double survival =
      std::clamp(std::max({throughput.r(), throughput.g(), throughput.b()}), 0.05, 0.95);
    const double roulette = state.sampleStream->sample1D(SampleDimension::Continuation,
                                                         static_cast<std::uint64_t>(bounce));
    if (roulette >= survival) {
      return false;
    }

    throughput = throughput * (1.0 / survival);
    return true;
  }

  void PathTracingIntegrator::recordDepthDelta(BatchDepthMetrics& depthMetrics,
                                               const Colord& before, const Colord& after) const {
    if (!depthMetrics.trackRadianceDelta) {
      return;
    }

    const double deltaSquared = radianceDeltaSquared(before, after);
    depthMetrics.depthDeltaSquaredSum += deltaSquared;
    if (depthMetrics.metrics) {
      depthMetrics.depthMaxDelta = std::max(depthMetrics.depthMaxDelta, std::sqrt(deltaSquared));
    }
  }

  void PathTracingIntegrator::recordFrontierHit(std::size_t pathIndex, BatchPath& path,
                                                const Primitive& primitive,
                                                const HitPoint& hitPoint, int bounce,
                                                BatchDepthMetrics& depthMetrics,
                                                std::vector<BatchHit>& activeHits) const {
    if (depthMetrics.trackFrontierMetrics()) {
      ++depthMetrics.frontierRayHits;
    }
    if (bounce == 0) {
      path.state.hitPoint = hitPoint;
    }
    activeHits.push_back(BatchHit{pathIndex, &primitive, hitPoint});
  }

  void PathTracingIntegrator::recordFrontierMiss(const Scene& scene, BatchPath& path,
                                                 BatchDepthMetrics& depthMetrics,
                                                 const Colord& accumulatedBeforeDepth) const {
    if (depthMetrics.trackFrontierMetrics()) {
      ++depthMetrics.frontierRayMisses;
    }
    path.accumulated() += path.throughput * missRadiance(scene, path.backgroundVisible);
    path.state.recurseOut();
    recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated());
  }

  void PathTracingIntegrator::intersectActivePathScalar(const Scene& scene, std::size_t pathIndex,
                                                        std::vector<BatchPath>& paths,
                                                        std::vector<BatchHit>& activeHits,
                                                        int bounce, BatchDepthMetrics& depthMetrics,
                                                        IntegratorBatchMetrics* metrics) const {
    auto& path = paths[pathIndex];
    const Colord accumulatedBeforeDepth =
      depthMetrics.trackRadianceDelta ? path.accumulated() : Colord::black();

    {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      path.state.recurseIn();
      if (depthMetrics.trackFrontierMetrics()) {
        ++depthMetrics.frontierScalarRays;
      }
    }

    HitPointInterval hitPoints;
    const Primitive* primitive = nullptr;
    {
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      primitive = scene.intersect(path.ray, hitPoints, path.state);
    }

    core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
    if (!primitive) {
      recordFrontierMiss(scene, path, depthMetrics, accumulatedBeforeDepth);
      return;
    }

    recordFrontierHit(pathIndex, path, *primitive, hitPoints.minWithPositiveDistance(), bounce,
                      depthMetrics, activeHits);
  }

  void PathTracingIntegrator::intersectActivePathPacket(
    const Scene& scene, std::size_t firstPathIndex, std::size_t laneCount,
    std::vector<BatchPath>& paths, std::vector<BatchHit>& activeHits, int bounce,
    BatchDepthMetrics& depthMetrics, IntegratorBatchMetrics* metrics) const {
    if (laneCount > Ray4::lanes) {
      throw std::logic_error("Ray4 path packet lane count exceeds packet width");
    }
    const std::size_t packetLaneCount = std::min(laneCount, Ray4::lanes);
    std::array<Rayd, Ray4::lanes> rays{Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                       Rayd::undefined};
    std::array<Colord, Ray4::lanes> accumulatedBeforeDepths;
    std::optional<std::array<std::map<std::string, std::uint64_t>, Ray4::lanes>>
      packetFallbacksBefore;
    PrimitivePacketState4 states{};

    PrimitivePacketHit4 packetHits;
    {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      if (depthMetrics.trackFrontierMetrics()) {
        ++depthMetrics.frontierPacketChunks;
        ++depthMetrics.frontierRay4PacketChunks;
        depthMetrics.frontierPacketRays += packetLaneCount;
        packetFallbacksBefore.emplace();
      }
      for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
        if (lane == packetLaneCount)
          break;
        const std::size_t pathIndex = firstPathIndex + lane;
        auto& path = paths[pathIndex];
        if (depthMetrics.trackRadianceDelta) {
          accumulatedBeforeDepths[lane] = path.accumulated();
        }
        path.state.recurseIn();
        if (depthMetrics.trackFrontierMetrics()) {
          (*packetFallbacksBefore)[lane] = path.state.packetHitScalarFallbacksByReason;
        }
        rays[lane] = path.ray;
        states[lane] = &path.state;
      }
    }

    {
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      packetHits = scene.intersectPacketHits(Ray4(rays), states);
    }

    core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      if (lane == packetLaneCount)
        break;
      const std::size_t pathIndex = firstPathIndex + lane;
      auto& path = paths[pathIndex];
      if (depthMetrics.trackFrontierMetrics()) {
        depthMetrics.recordPacketScalarFallbacks(path.state, (*packetFallbacksBefore)[lane]);
      }
      if (!packetHits.hit(lane)) {
        recordFrontierMiss(scene, path, depthMetrics, accumulatedBeforeDepths[lane]);
        continue;
      }

      recordFrontierHit(pathIndex, path, *packetHits.primitive(lane), packetHits.hitPoint(lane),
                        bounce, depthMetrics, activeHits);
    }
  }

  void PathTracingIntegrator::intersectActivePathPacket8(
    const Scene& scene, std::size_t firstPathIndex, std::size_t laneCount,
    std::vector<BatchPath>& paths, std::vector<BatchHit>& activeHits, int bounce,
    BatchDepthMetrics& depthMetrics, IntegratorBatchMetrics* metrics) const {
    if (laneCount > Ray8::lanes) {
      throw std::logic_error("Ray8 path packet lane count exceeds packet width");
    }
    const std::size_t packetLaneCount = std::min(laneCount, Ray8::lanes);
    std::array<Rayd, Ray8::lanes> rays{Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                       Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                       Rayd::undefined, Rayd::undefined};
    std::array<Colord, Ray8::lanes> accumulatedBeforeDepths;
    std::optional<std::array<std::map<std::string, std::uint64_t>, Ray8::lanes>>
      packetFallbacksBefore;
    PrimitivePacketState8 states{};

    PrimitivePacketHit8 packetHits;
    {
      core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
      if (depthMetrics.trackFrontierMetrics()) {
        ++depthMetrics.frontierPacketChunks;
        ++depthMetrics.frontierRay8PacketChunks;
        depthMetrics.frontierPacketRays += packetLaneCount;
        packetFallbacksBefore.emplace();
      }
      for (std::size_t lane = 0; lane != Ray8::lanes; ++lane) {
        if (lane == packetLaneCount)
          break;
        const std::size_t pathIndex = firstPathIndex + lane;
        auto& path = paths[pathIndex];
        if (depthMetrics.trackRadianceDelta) {
          accumulatedBeforeDepths[lane] = path.accumulated();
        }
        path.state.recurseIn();
        if (depthMetrics.trackFrontierMetrics()) {
          (*packetFallbacksBefore)[lane] = path.state.packetHitScalarFallbacksByReason;
        }
        rays[lane] = path.ray;
        states[lane] = &path.state;
      }
    }

    {
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      packetHits = scene.intersectPacketHits(Ray8(rays), states);
    }

    core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
    for (std::size_t lane = 0; lane != Ray8::lanes; ++lane) {
      if (lane == packetLaneCount)
        break;
      const std::size_t pathIndex = firstPathIndex + lane;
      auto& path = paths[pathIndex];
      if (depthMetrics.trackFrontierMetrics()) {
        depthMetrics.recordPacketScalarFallbacks(path.state, (*packetFallbacksBefore)[lane]);
      }
      if (!packetHits.hit(lane)) {
        recordFrontierMiss(scene, path, depthMetrics, accumulatedBeforeDepths[lane]);
        continue;
      }

      recordFrontierHit(pathIndex, path, *packetHits.primitive(lane), packetHits.hitPoint(lane),
                        bounce, depthMetrics, activeHits);
    }
  }

  void PathTracingIntegrator::intersectActiveFrontier(const Scene& scene,
                                                      std::vector<BatchPath>& paths,
                                                      std::vector<BatchHit>& activeHits, int bounce,
                                                      BatchDepthMetrics& depthMetrics,
                                                      IntegratorBatchMetrics* metrics) const {
    activeHits.clear();

    std::size_t activeIndex = 0;
    while (activeIndex != paths.size()) {
      if (activeIndex + Ray8::lanes <= paths.size()) {
        intersectActivePathPacket8(scene, activeIndex, Ray8::lanes, paths, activeHits, bounce,
                                   depthMetrics, metrics);
        activeIndex += Ray8::lanes;
        continue;
      }

      const std::size_t remainingPaths = paths.size() - activeIndex;
      if (remainingPaths > Ray4::lanes) {
        intersectActivePathPacket8(scene, activeIndex, remainingPaths, paths, activeHits, bounce,
                                   depthMetrics, metrics);
        activeIndex += remainingPaths;
        continue;
      }

      if (activeIndex + Ray4::lanes <= paths.size()) {
        intersectActivePathPacket(scene, activeIndex, Ray4::lanes, paths, activeHits, bounce,
                                  depthMetrics, metrics);
        activeIndex += Ray4::lanes;
        continue;
      }

      if (remainingPaths > 1) {
        intersectActivePathPacket(scene, activeIndex, remainingPaths, paths, activeHits, bounce,
                                  depthMetrics, metrics);
        activeIndex += remainingPaths;
        continue;
      }

      intersectActivePathScalar(scene, activeIndex, paths, activeHits, bounce, depthMetrics,
                                metrics);
      ++activeIndex;
    }
  }

  void PathTracingIntegrator::retainActivePath(std::vector<BatchPath>& paths, std::size_t pathIndex,
                                               std::size_t& retainedPathCount) const {
    if (pathIndex != retainedPathCount) {
      paths[retainedPathCount] = std::move(paths[pathIndex]);
    }
    ++retainedPathCount;
  }

  Colord PathTracingIntegrator::directLighting(const Scene& scene, const Light& light,
                                               const HitPoint& hitPoint, const Material& material,
                                               const Vector3d& wi, State& state) const {
    LightSample sample = light.sample(hitPoint.point());
    if (sample.pdf <= 0.0 || sample.radiance == Colord::black()) {
      return Colord::black();
    }

    const Vector3d wo = sample.direction;
    const double normalDotOut = hitPoint.normal() * wo;
    if (normalDotOut <= 0.0) {
      return Colord::black();
    }

    // Shadow ray. `Scene::occludes` keeps point-light visibility bounded
    // to the sampled light distance; epsilon-shift avoids self-intersection.
    const Rayd shadowRay = Rayd(hitPoint.point(), wo).epsilonShifted();
    if (scene.occludes(shadowRay, state, sample.distance)) {
      state.shadowHit(nullptr, "PathTracingIntegrator");
      return Colord::black();
    }
    state.shadowMiss(nullptr, "PathTracingIntegrator");

    const Colord bsdfValue = material.evalBsdf(hitPoint, wi, wo);
    if (bsdfValue == Colord::black()) {
      return Colord::black();
    }

    const double bsdfPdf = sample.delta ? 0.0 : material.bsdfPdf(hitPoint, wi, wo);
    return mis::estimateDirectLightingFromLightSample(bsdfValue, sample.radiance, normalDotOut,
                                                      sample.pdf, bsdfPdf, sample.delta);
  }

  Colord PathTracingIntegrator::radiance(const Scene& scene, const Rayd& primaryRay, State& state,
                                         const RayCaster& recursiveRayCaster) const {
    if (state.sampleStream == nullptr) {
      state.recordEvent(nullptr,
                        "PathTracing: no sample stream on state — falling back to Whitted");
      // Recursive Whitted-style entry. Lets unit tests that construct a
      // bare `State{}` still get a useful color out of the integrator
      // for smoke purposes, even though they aren't really sampling.
      return recursiveRayCaster.rayColor(primaryRay, state);
    }

    Colord accumulated = Colord::black();
    std::vector<ScalarPath> paths;
    paths.emplace_back(primaryRay, state);

    for (int bounce = 0; bounce < m_maximumRecursionDepth && !paths.empty(); ++bounce) {
      std::vector<ScalarPath> nextPaths;
      nextPaths.reserve(paths.size() * 2);

      for (auto& path : paths) {
        if (isCancelled()) {
          return accumulated;
        }

        State& pathState = path.pathState();
        pathState.recurseIn();

        HitPointInterval hitPoints;
        const Primitive* primitive = scene.intersect(path.ray, hitPoints, pathState);
        if (!primitive) {
          accumulated += path.throughput * missRadiance(scene, path.backgroundVisible);
          pathState.recurseOut();
          continue;
        }

        const HitPoint hitPoint = hitPoints.minWithPositiveDistance();
        if (bounce == 0) {
          pathState.hitPoint = hitPoint;
        }

        const auto material = primitive->material();
        if (!material) {
          pathState.recurseOut();
          continue;
        }

        // wi is the direction back along the incoming ray, pointing
        // AWAY from the surface — matches the BSDF convention.
        const Vector3d wi = -path.ray.direction().normalized();

        // Materials without BSDF support fall back to Whitted. The
        // contribution is the full shaded color (which includes direct
        // lighting); we add it weighted by throughput and terminate
        // this path. No further bounces past such a surface yet.
        if (!material->supportsBsdfSampling()) {
          const Colord whittedColor =
            material->shade(&recursiveRayCaster, scene, path.ray, hitPoint, pathState);
          accumulated += path.throughput * whittedColor;
          pathState.recurseOut();
          continue;
        }

        accumulated += path.throughput * material->ambientRadiance(scene, path.ray, hitPoint);

        // Direct lighting via NEE.
        for (const auto& light : scene.lights()) {
          accumulated +=
            path.throughput * directLighting(scene, *light, hitPoint, *material, wi, pathState);
        }

        const std::vector<MaterialBsdfSample> deltaSamples =
          material->deltaBsdfSamples(hitPoint, wi);
        if (!deltaSamples.empty()) {
          State baseState = clonePathState(pathState);
          baseState.recurseOut();
          for (const MaterialBsdfSample& sampled : deltaSamples) {
            if (!canContinueWithSample(sampled, hitPoint)) {
              continue;
            }

            Colord nextThroughput = continuedThroughput(path.throughput, sampled, hitPoint);
            State childState = clonePathState(baseState);
            if (!survivesRussianRoulette(nextThroughput, childState, bounce)) {
              continue;
            }
            nextPaths.emplace_back(sampled.rayFrom(hitPoint), nextThroughput,
                                   path.backgroundVisible, std::move(childState));
          }
          pathState.recurseOut();
          continue;
        }

        // Indirect: sample a continuation direction.
        const Vector2d bsdfSample = pathState.sampleStream->sample2D(
          SampleDimension::BSDF, static_cast<std::uint64_t>(bounce));
        const MaterialBsdfSample sampled = material->sampleBsdf(hitPoint, wi, bsdfSample);
        if (!canContinueWithSample(sampled, hitPoint)) {
          pathState.recurseOut();
          continue;
        }

        Colord nextThroughput = continuedThroughput(path.throughput, sampled, hitPoint);
        if (!survivesRussianRoulette(nextThroughput, pathState, bounce)) {
          pathState.recurseOut();
          continue;
        }

        State childState = clonePathState(pathState);
        childState.recurseOut();
        pathState.recurseOut();
        nextPaths.emplace_back(sampled.rayFrom(hitPoint), nextThroughput,
                               sampled.isDelta ? path.backgroundVisible : false,
                               std::move(childState));
      }

      paths = std::move(nextPaths);
    }

    return accumulated;
  }

  std::vector<Colord> PathTracingIntegrator::radianceBatch(
    const Scene& scene, const std::vector<IntegratorRaySample>& samples,
    const RayCaster& recursiveRayCaster, IntegratorBatchMetrics* metrics,
    const IntegratorBatchSettings& settings) const {
    if (metrics) {
      metrics->reset(/*scalarFallback=*/false);
    }

    std::vector<Colord> sampleColors;
    std::vector<BatchPath> paths;
    {
      core::util::ScopedTimer timer(metrics ? &metrics->pathSetupWorkerSeconds : nullptr);
      sampleColors.resize(samples.size(), Colord::black());
      paths.reserve(samples.size());
      for (std::size_t index = 0; index != samples.size(); ++index) {
        const auto& sample = samples[index];
        if (!sample.sampleStream()) {
          return Integrator::radianceBatch(scene, samples, recursiveRayCaster, metrics, settings);
        }

        paths.emplace_back(sample, sampleColors[index]);
      }
    }

    const bool trackRadianceDelta = metrics || settings.convergenceEnabled;
    const std::uint64_t totalSampleCount = sampleColors.size();
    std::vector<BatchHit> activeHits;
    activeHits.reserve(samples.size());

    for (int bounce = 0; bounce < m_maximumRecursionDepth; ++bounce) {
      const std::uint64_t activeCount = paths.size();
      if (activeCount == 0) {
        break;
      }
      if (metrics) {
        metrics->recordActiveDepth(activeCount);
      }

      std::size_t retainedPathCount = 0;
      BatchDepthMetrics depthMetrics;
      depthMetrics.trackRadianceDelta = trackRadianceDelta;
      depthMetrics.metrics = metrics;
      intersectActiveFrontier(scene, paths, activeHits, bounce, depthMetrics, metrics);
      if (metrics) {
        metrics->recordFrontierIntersections(depthMetrics.frontierRayHits,
                                             depthMetrics.frontierRayMisses);
        metrics->recordFrontierTraversal(
          depthMetrics.frontierPacketChunks, depthMetrics.frontierPacketRays,
          depthMetrics.frontierRay4PacketChunks, depthMetrics.frontierRay8PacketChunks,
          depthMetrics.frontierScalarRays, depthMetrics.frontierPacketScalarFallbackRays,
          /*packetRefinedRays=*/0);
        metrics->recordPacketScalarFallbacksByReason(
          depthMetrics.frontierPacketScalarFallbackRaysByReason);
      }

      std::vector<BatchPath> spawnedPaths;
      spawnedPaths.reserve(activeHits.size() * 2);

      for (const auto& hit : activeHits) {
        auto& path = paths[hit.pathIndex];
        const Colord accumulatedBeforeDepth =
          depthMetrics.trackRadianceDelta ? path.accumulated() : Colord::black();
        {
          core::util::ScopedTimer timer(metrics ? &metrics->shadingWorkerSeconds : nullptr);
          const auto material = hit.primitive->material();
          if (!material) {
            path.state.recurseOut();
            recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated());
            continue;
          }

          const Vector3d wi = -path.ray.direction().normalized();
          if (!material->supportsBsdfSampling()) {
            const Colord whittedColor =
              material->shade(&recursiveRayCaster, scene, path.ray, hit.hitPoint, path.state);
            path.accumulated() += path.throughput * whittedColor;
            if (metrics) {
              ++metrics->compatibilityShadeSamples;
            }
            path.state.recurseOut();
            recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated());
            continue;
          }

          path.accumulated() +=
            path.throughput * material->ambientRadiance(scene, path.ray, hit.hitPoint);

          for (const auto& light : scene.lights()) {
            path.accumulated() += path.throughput * directLighting(scene, *light, hit.hitPoint,
                                                                   *material, wi, path.state);
          }

          const std::vector<MaterialBsdfSample> deltaSamples =
            material->deltaBsdfSamples(hit.hitPoint, wi);
          if (!deltaSamples.empty()) {
            State baseState = clonePathState(path.state);
            baseState.recurseOut();
            for (const MaterialBsdfSample& sampled : deltaSamples) {
              if (!canContinueWithSample(sampled, hit.hitPoint)) {
                continue;
              }

              Colord nextThroughput = continuedThroughput(path.throughput, sampled, hit.hitPoint);
              State childState = clonePathState(baseState);
              if (!survivesRussianRoulette(nextThroughput, childState, bounce)) {
                continue;
              }
              spawnedPaths.emplace_back(sampled.rayFrom(hit.hitPoint), nextThroughput,
                                        path.backgroundVisible, std::move(childState),
                                        path.accumulated());
            }
            path.state.recurseOut();
            recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated());
            continue;
          }

          const Vector2d bsdfSample = path.state.sampleStream->sample2D(
            SampleDimension::BSDF, static_cast<std::uint64_t>(bounce));
          const MaterialBsdfSample sampled = material->sampleBsdf(hit.hitPoint, wi, bsdfSample);
          if (!canContinueWithSample(sampled, hit.hitPoint)) {
            path.state.recurseOut();
            recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated());
            continue;
          }

          path.throughput = continuedThroughput(path.throughput, sampled, hit.hitPoint);
          if (!sampled.isDelta) {
            path.backgroundVisible = false;
          }

          if (!survivesRussianRoulette(path.throughput, path.state, bounce)) {
            path.state.recurseOut();
            recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated());
            continue;
          }

          path.ray = sampled.rayFrom(hit.hitPoint);
          path.state.recurseOut();
          recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated());
          retainActivePath(paths, hit.pathIndex, retainedPathCount);
        }
      }

      while (paths.size() != retainedPathCount) {
        paths.pop_back();
      }
      for (auto& spawnedPath : spawnedPaths) {
        paths.push_back(std::move(spawnedPath));
      }
      retainedPathCount = paths.size();

      if (metrics) {
        metrics->recordRadianceDeltaDepth(depthMetrics.depthDeltaSquaredSum,
                                          depthMetrics.depthMaxDelta);
        metrics->recordRetainedActiveDepth(retainedPathCount);
      }

      IntegratorBatchFeedback feedback;
      if (settings.progressObserver) {
        core::util::ScopedTimer timer(metrics ? &metrics->progressSnapshotWorkerSeconds : nullptr);
        feedback = settings.progressObserver->depthCompleted(static_cast<std::uint64_t>(bounce + 1),
                                                             sampleColors, retainedPathCount);
      }

      if (settings.convergenceEnabled && totalSampleCount != 0) {
        core::util::ScopedTimer timer(metrics ? &metrics->convergenceTestWorkerSeconds : nullptr);
        const double activeFraction =
          static_cast<double>(retainedPathCount) / static_cast<double>(totalSampleCount);
        const double rawRadianceDeltaRms =
          activeCount == 0
            ? 0.0
            : std::sqrt(depthMetrics.depthDeltaSquaredSum / static_cast<double>(activeCount));
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
    }

    return sampleColors;
  }
}
