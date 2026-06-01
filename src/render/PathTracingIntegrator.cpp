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

namespace render {
  struct PathTracingIntegrator::BatchPath {
    explicit BatchPath(const IntegratorRaySample& sample)
        : ray(sample.ray) {
      state.timeSample = sample.timeSample;
      state.sampleStream = sample.sampleStream();
    }

    Rayd ray;
    Colord throughput{Colord::white()};
    Colord accumulated{Colord::black()};
    State state;
    bool active{true};
  };

  struct PathTracingIntegrator::BatchHit {
    std::size_t pathIndex{0};
    const Primitive* primitive{nullptr};
    HitPoint hitPoint;
    Colord accumulatedBeforeDepth{Colord::black()};
  };

  struct PathTracingIntegrator::BatchDepthMetrics {
    bool trackRadianceDelta{false};
    std::uint64_t frontierRayHits{0};
    std::uint64_t frontierRayMisses{0};
    std::uint64_t frontierPacketChunks{0};
    std::uint64_t frontierScalarRays{0};
    std::uint64_t frontierPacketScalarFallbackRays{0};
    double depthDeltaSquaredSum{0.0};
    double depthMaxDelta{0.0};
    IntegratorBatchMetrics* metrics{nullptr};
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
                                                std::vector<BatchHit>& activeHits,
                                                const Colord& accumulatedBeforeDepth) const {
    ++depthMetrics.frontierRayHits;
    if (bounce == 0) {
      path.state.hitPoint = hitPoint;
    }
    activeHits.push_back(BatchHit{pathIndex, &primitive, hitPoint, accumulatedBeforeDepth});
  }

  void PathTracingIntegrator::recordFrontierMiss(const Scene& scene, BatchPath& path,
                                                 BatchDepthMetrics& depthMetrics,
                                                 const Colord& accumulatedBeforeDepth) const {
    ++depthMetrics.frontierRayMisses;
    path.accumulated += path.throughput * scene.background();
    path.state.recurseOut();
    path.active = false;
    recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated);
  }

  void PathTracingIntegrator::intersectActivePathScalar(const Scene& scene, std::size_t pathIndex,
                                                        std::vector<BatchPath>& paths,
                                                        std::vector<BatchHit>& activeHits,
                                                        int bounce, BatchDepthMetrics& depthMetrics,
                                                        IntegratorBatchMetrics* metrics) const {
    auto& path = paths[pathIndex];
    const Colord accumulatedBeforeDepth = path.accumulated;
    if (isCancelled()) {
      path.active = false;
      recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated);
      return;
    }

    path.state.recurseIn();
    ++depthMetrics.frontierScalarRays;

    HitPointInterval hitPoints;
    const Primitive* primitive = nullptr;
    {
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      primitive = scene.intersect(path.ray, hitPoints, path.state);
    }
    if (!primitive) {
      recordFrontierMiss(scene, path, depthMetrics, accumulatedBeforeDepth);
      return;
    }

    recordFrontierHit(pathIndex, path, *primitive, hitPoints.minWithPositiveDistance(), bounce,
                      depthMetrics, activeHits, accumulatedBeforeDepth);
  }

  void PathTracingIntegrator::intersectActivePathPacket(
    const Scene& scene, const std::vector<std::size_t>& activePathIndices,
    std::size_t firstActivePathIndex, std::vector<BatchPath>& paths,
    std::vector<BatchHit>& activeHits, int bounce, BatchDepthMetrics& depthMetrics,
    IntegratorBatchMetrics* metrics) const {
    std::array<Rayd, Ray4::lanes> rays{Rayd::undefined, Rayd::undefined, Rayd::undefined,
                                       Rayd::undefined};
    std::array<Colord, Ray4::lanes> accumulatedBeforeDepths;
    std::array<std::uint64_t, Ray4::lanes> packetFallbacksBefore{};
    PrimitivePacketState4 states{};

    ++depthMetrics.frontierPacketChunks;
    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      const std::size_t pathIndex = activePathIndices[firstActivePathIndex + lane];
      auto& path = paths[pathIndex];
      accumulatedBeforeDepths[lane] = path.accumulated;
      path.state.recurseIn();
      packetFallbacksBefore[lane] = path.state.packetHitScalarFallbacks;
      rays[lane] = path.ray;
      states[lane] = &path.state;
    }

    PrimitivePacketHit4 packetHits;
    {
      core::util::ScopedTimer timer(metrics ? &metrics->intersectionWorkerSeconds : nullptr);
      packetHits = scene.intersectPacketHits(Ray4(rays), states);
    }

    for (std::size_t lane = 0; lane != Ray4::lanes; ++lane) {
      const std::size_t pathIndex = activePathIndices[firstActivePathIndex + lane];
      auto& path = paths[pathIndex];
      depthMetrics.frontierPacketScalarFallbackRays +=
        path.state.packetHitScalarFallbacks - packetFallbacksBefore[lane];
      if (!packetHits.hit(lane)) {
        recordFrontierMiss(scene, path, depthMetrics, accumulatedBeforeDepths[lane]);
        continue;
      }

      recordFrontierHit(pathIndex, path, *packetHits.primitive(lane), packetHits.hitPoint(lane),
                        bounce, depthMetrics, activeHits, accumulatedBeforeDepths[lane]);
    }
  }

  void PathTracingIntegrator::intersectActiveFrontier(
    const Scene& scene, const std::vector<std::size_t>& activePathIndices,
    std::vector<BatchPath>& paths, std::vector<BatchHit>& activeHits, int bounce,
    BatchDepthMetrics& depthMetrics, IntegratorBatchMetrics* metrics) const {
    activeHits.clear();

    std::size_t activeIndex = 0;
    while (activeIndex != activePathIndices.size()) {
      if (isCancelled()) {
        const std::size_t pathIndex = activePathIndices[activeIndex];
        auto& path = paths[pathIndex];
        const Colord accumulatedBeforeDepth = path.accumulated;
        path.active = false;
        recordDepthDelta(depthMetrics, accumulatedBeforeDepth, path.accumulated);
        ++activeIndex;
        continue;
      }

      if (activeIndex + Ray4::lanes <= activePathIndices.size()) {
        intersectActivePathPacket(scene, activePathIndices, activeIndex, paths, activeHits, bounce,
                                  depthMetrics, metrics);
        activeIndex += Ray4::lanes;
      } else {
        intersectActivePathScalar(scene, activePathIndices[activeIndex], paths, activeHits, bounce,
                                  depthMetrics, metrics);
        ++activeIndex;
      }
    }
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
    std::vector<BatchPath> paths;
    paths.reserve(samples.size());
    std::vector<std::size_t> activePathIndices;
    activePathIndices.reserve(samples.size());
    for (const auto& sample : samples) {
      if (!sample.sampleStream()) {
        return Integrator::radianceBatch(scene, samples, recursiveRayCaster, metrics, settings);
      }

      paths.emplace_back(sample);
      activePathIndices.push_back(paths.size() - 1);
    }

    if (metrics) {
      metrics->reset(/*scalarFallback=*/false);
    }
    const bool trackRadianceDelta = metrics || settings.convergenceEnabled;
    std::vector<std::size_t> nextActivePathIndices;
    nextActivePathIndices.reserve(samples.size());
    std::vector<BatchHit> activeHits;
    activeHits.reserve(samples.size());

    for (int bounce = 0; bounce < m_maximumRecursionDepth; ++bounce) {
      const std::uint64_t activeCount = activePathIndices.size();
      if (activeCount == 0) {
        break;
      }
      if (metrics) {
        metrics->recordActiveDepth(activeCount);
      }

      nextActivePathIndices.clear();
      BatchDepthMetrics depthMetrics;
      depthMetrics.trackRadianceDelta = trackRadianceDelta;
      depthMetrics.metrics = metrics;
      intersectActiveFrontier(scene, activePathIndices, paths, activeHits, bounce, depthMetrics,
                              metrics);
      if (metrics) {
        metrics->recordFrontierIntersections(depthMetrics.frontierRayHits,
                                             depthMetrics.frontierRayMisses);
        metrics->recordFrontierTraversal(depthMetrics.frontierPacketChunks,
                                         depthMetrics.frontierScalarRays,
                                         depthMetrics.frontierPacketScalarFallbackRays);
      }

      for (const auto& hit : activeHits) {
        auto& path = paths[hit.pathIndex];
        {
          core::util::ScopedTimer timer(metrics ? &metrics->shadingWorkerSeconds : nullptr);
          const auto material = hit.primitive->material();
          if (!material) {
            path.state.recurseOut();
            path.active = false;
            recordDepthDelta(depthMetrics, hit.accumulatedBeforeDepth, path.accumulated);
            continue;
          }

          const Vector3d wi = -path.ray.direction().normalized();
          if (!material->supportsBsdfSampling()) {
            const Colord whittedColor =
              material->shade(&recursiveRayCaster, scene, path.ray, hit.hitPoint, path.state);
            path.accumulated += path.throughput * whittedColor;
            if (metrics) {
              ++metrics->compatibilityShadeSamples;
            }
            path.state.recurseOut();
            path.active = false;
            recordDepthDelta(depthMetrics, hit.accumulatedBeforeDepth, path.accumulated);
            continue;
          }

          for (const auto& light : scene.lights()) {
            path.accumulated += path.throughput * directLighting(scene, *light, hit.hitPoint,
                                                                 *material, wi, path.state);
          }

          const Vector2d bsdfSample = path.state.sampleStream->sample2D(
            SampleDimension::BSDF, static_cast<std::uint64_t>(bounce));
          const MaterialBsdfSample sampled = material->sampleBsdf(hit.hitPoint, wi, bsdfSample);
          if (sampled.pdf <= 0.0 || sampled.value == Colord::black()) {
            path.state.recurseOut();
            path.active = false;
            recordDepthDelta(depthMetrics, hit.accumulatedBeforeDepth, path.accumulated);
            continue;
          }

          const double normalDotWo = hit.hitPoint.normal() * sampled.direction;
          if (!sampled.isDelta && normalDotWo <= 0.0) {
            path.state.recurseOut();
            path.active = false;
            recordDepthDelta(depthMetrics, hit.accumulatedBeforeDepth, path.accumulated);
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
              path.active = false;
              recordDepthDelta(depthMetrics, hit.accumulatedBeforeDepth, path.accumulated);
              continue;
            }
            path.throughput = path.throughput * (1.0 / survival);
          }

          path.ray = sampled.rayFrom(hit.hitPoint);
          path.state.recurseOut();
          recordDepthDelta(depthMetrics, hit.accumulatedBeforeDepth, path.accumulated);
          nextActivePathIndices.push_back(hit.pathIndex);
        }
      }

      if (metrics) {
        metrics->recordRadianceDeltaDepth(depthMetrics.depthDeltaSquaredSum,
                                          depthMetrics.depthMaxDelta);
      }

      if (settings.progressObserver) {
        std::vector<Colord> snapshot;
        snapshot.reserve(paths.size());
        for (const auto& path : paths) {
          snapshot.push_back(path.accumulated);
        }
        settings.progressObserver->depthCompleted(static_cast<std::uint64_t>(bounce + 1), snapshot,
                                                  nextActivePathIndices.size());
      }

      if (settings.convergenceEnabled && !paths.empty()) {
        const double activeFraction =
          static_cast<double>(nextActivePathIndices.size()) / static_cast<double>(paths.size());
        const double radianceDeltaRms =
          activeCount == 0
            ? 0.0
            : std::sqrt(depthMetrics.depthDeltaSquaredSum / static_cast<double>(activeCount));
        if (activeFraction <= settings.activeSampleFractionThreshold &&
            radianceDeltaRms <= settings.radianceDeltaRmsThreshold) {
          if (metrics) {
            metrics->stoppedByConvergence = true;
            metrics->stoppedAfterDepth = metrics->activeSamplesPerDepth.size();
          }
          break;
        }
      }

      activePathIndices.swap(nextActivePathIndices);
    }

    std::vector<Colord> result;
    result.reserve(paths.size());
    for (const auto& path : paths) {
      result.push_back(path.accumulated);
    }
    return result;
  }
}
