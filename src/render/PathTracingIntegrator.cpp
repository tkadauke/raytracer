#include "render/PathTracingIntegrator.h"

#include "core/math/HitPoint.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/util/ScopedTimer.h"
#include "render/RayCaster.h"
#include "render/State.h"
#include "render/lights/Light.h"
#include "render/materials/Material.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"
#include "render/samplers/SampleStream.h"

#include <algorithm>
#include <cmath>
#include <map>
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

    Colord& accumulated() {
      return *m_accumulated;
    }

    const Colord& accumulated() const {
      return *m_accumulated;
    }

    Rayd ray;
    Colord throughput{Colord::white()};
    State state;

  private:
    Colord* m_accumulated;
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
    path.accumulated() += path.throughput * scene.background();
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
      if (isCancelled()) {
        recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated());
        return;
      }

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
    std::array<Rayd, Ray4::lanes> rays{Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                       Rayd::undefined};
    const std::size_t packetLaneCount = std::min(laneCount, Ray4::lanes);
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
      const auto prepareLane = [&](std::size_t lane) {
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
      };
      if (packetLaneCount > 0) {
        prepareLane(0);
      }
      if (packetLaneCount > 1) {
        prepareLane(1);
      }
      if (packetLaneCount > 2) {
        prepareLane(2);
      }
      if (packetLaneCount > 3) {
        prepareLane(3);
      }
    }

    {
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      packetHits = scene.intersectPacketHits(Ray4(rays), states);
    }

    core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
    for (std::size_t lane = 0; lane != packetLaneCount; ++lane) {
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
    std::array<Rayd, Ray8::lanes> rays{Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                       Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                       Rayd::undefined, Rayd::undefined};
    const std::size_t packetLaneCount = std::min(laneCount, Ray8::lanes);
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
      const auto prepareLane = [&](std::size_t lane) {
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
      };
      if (packetLaneCount > 0) {
        prepareLane(0);
      }
      if (packetLaneCount > 1) {
        prepareLane(1);
      }
      if (packetLaneCount > 2) {
        prepareLane(2);
      }
      if (packetLaneCount > 3) {
        prepareLane(3);
      }
      if (packetLaneCount > 4) {
        prepareLane(4);
      }
      if (packetLaneCount > 5) {
        prepareLane(5);
      }
      if (packetLaneCount > 6) {
        prepareLane(6);
      }
      if (packetLaneCount > 7) {
        prepareLane(7);
      }
    }

    {
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      packetHits = scene.intersectPacketHits(Ray8(rays), states);
    }

    core::util::ScopedTimer timer(metrics ? &metrics->frontierBookkeepingWorkerSeconds : nullptr);
    for (std::size_t lane = 0; lane != packetLaneCount; ++lane) {
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

    // Delta lights: pdf encodes the discrete sample probability; the
    // value `radiance / pdf` is the correct contribution. Finite-PDF
    // area lights would add a MIS weight here; we have none yet so the
    // branch is the same in both cases.
    return bsdfValue * sample.radiance * (normalDotOut / sample.pdf);
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
    Colord throughput = Colord::white();
    Rayd ray = primaryRay;

    for (int bounce = 0; bounce < m_maximumRecursionDepth; ++bounce) {
      if (isCancelled()) {
        return scene.background();
      }

      state.recurseIn();

      HitPointInterval hitPoints;
      const Primitive* primitive = scene.intersect(ray, hitPoints, state);
      if (!primitive) {
        accumulated += throughput * scene.background();
        state.recurseOut();
        break;
      }

      const HitPoint hitPoint = hitPoints.minWithPositiveDistance();
      if (bounce == 0) {
        state.hitPoint = hitPoint;
      }

      const auto material = primitive->material();
      if (!material) {
        state.recurseOut();
        break;
      }

      // wi is the direction back along the incoming ray, pointing
      // AWAY from the surface — matches the BSDF convention.
      const Vector3d wi = -ray.direction().normalized();

      // Materials without BSDF support fall back to Whitted. The
      // contribution is the full shaded color (which includes direct
      // lighting); we add it weighted by throughput and terminate
      // this path. No further bounces past such a surface yet.
      if (!material->supportsBsdfSampling()) {
        const Colord whittedColor =
          material->shade(&recursiveRayCaster, scene, ray, hitPoint, state);
        accumulated += throughput * whittedColor;
        state.recurseOut();
        break;
      }

      // Direct lighting via NEE.
      for (const auto& light : scene.lights()) {
        accumulated += throughput * directLighting(scene, *light, hitPoint, *material, wi, state);
      }

      // Indirect: sample a continuation direction.
      const Vector2d bsdfSample =
        state.sampleStream->sample2D(SampleDimension::BSDF, static_cast<std::uint64_t>(bounce));
      const MaterialBsdfSample sampled = material->sampleBsdf(hitPoint, wi, bsdfSample);
      if (sampled.pdf <= 0.0 || sampled.value == Colord::black()) {
        state.recurseOut();
        break;
      }

      const double normalDotWo = hitPoint.normal() * sampled.direction;
      if (!sampled.isDelta && normalDotWo <= 0.0) {
        state.recurseOut();
        break;
      }

      // For delta lobes the value is already the post-cancellation
      // contribution; skip the pdf division and the cosine.
      if (sampled.isDelta) {
        throughput = throughput * sampled.value;
      } else {
        throughput = throughput * (sampled.value * (normalDotWo / sampled.pdf));
      }

      // Russian roulette beyond the configured depth.
      if (bounce >= m_russianRouletteDepth) {
        const double survival =
          std::clamp(std::max({throughput.r(), throughput.g(), throughput.b()}), 0.05, 0.95);
        const double roulette = state.sampleStream->sample1D(SampleDimension::Continuation,
                                                             static_cast<std::uint64_t>(bounce));
        if (roulette >= survival) {
          state.recurseOut();
          break;
        }
        throughput = throughput * (1.0 / survival);
      }

      // Continue along the sampled direction.
      ray = sampled.rayFrom(hitPoint);
      state.recurseOut();
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
      if (isCancelled()) {
        if (metrics) {
          metrics->recordRadianceDeltaDepth(depthMetrics.depthDeltaSquaredSum,
                                            depthMetrics.depthMaxDelta);
          metrics->recordRetainedActiveDepth(retainedPathCount);
        }
        break;
      }

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

          for (const auto& light : scene.lights()) {
            path.accumulated() += path.throughput * directLighting(scene, *light, hit.hitPoint,
                                                                   *material, wi, path.state);
          }

          const Vector2d bsdfSample = path.state.sampleStream->sample2D(
            SampleDimension::BSDF, static_cast<std::uint64_t>(bounce));
          const MaterialBsdfSample sampled = material->sampleBsdf(hit.hitPoint, wi, bsdfSample);
          if (sampled.pdf <= 0.0 || sampled.value == Colord::black()) {
            path.state.recurseOut();
            recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated());
            continue;
          }

          const double normalDotWo = hit.hitPoint.normal() * sampled.direction;
          if (!sampled.isDelta && normalDotWo <= 0.0) {
            path.state.recurseOut();
            recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated());
            continue;
          }

          if (sampled.isDelta) {
            path.throughput = path.throughput * sampled.value;
          } else {
            path.throughput = path.throughput * (sampled.value * (normalDotWo / sampled.pdf));
          }

          if (bounce >= m_russianRouletteDepth) {
            const double survival =
              std::clamp(std::max({path.throughput.r(), path.throughput.g(), path.throughput.b()}),
                         0.05, 0.95);
            const double roulette = path.state.sampleStream->sample1D(
              SampleDimension::Continuation, static_cast<std::uint64_t>(bounce));
            if (roulette >= survival) {
              path.state.recurseOut();
              recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated());
              continue;
            }
            path.throughput = path.throughput * (1.0 / survival);
          }

          path.ray = sampled.rayFrom(hit.hitPoint);
          path.state.recurseOut();
          recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated());
          retainActivePath(paths, hit.pathIndex, retainedPathCount);
        }
      }

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

      while (paths.size() != retainedPathCount) {
        paths.pop_back();
      }
    }

    return sampleColors;
  }
}
