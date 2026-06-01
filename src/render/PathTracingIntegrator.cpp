#include "render/PathTracingIntegrator.h"

#include "core/math/HitPoint.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
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
    struct PathState {
      explicit PathState(const IntegratorRaySample& sample)
          : ray(sample.ray) {
        state.timeSample = sample.timeSample;
        state.sampleStream = sample.sampleStream.get();
      }

      Rayd ray;
      Colord throughput{Colord::white()};
      Colord accumulated{Colord::black()};
      State state;
      bool active{true};
    };

    std::vector<PathState> paths;
    paths.reserve(samples.size());
    for (const auto& sample : samples) {
      if (!sample.sampleStream) {
        return Integrator::radianceBatch(scene, samples, recursiveRayCaster, metrics, settings);
      }

      paths.emplace_back(sample);
    }

    const auto activePathCount = [&paths] {
      std::uint64_t activeCount = 0;
      for (const auto& path : paths) {
        if (path.active) {
          ++activeCount;
        }
      }
      return activeCount;
    };

    if (metrics) {
      metrics->usedScalarFallback = false;
      metrics->activeSamplesPerDepth.clear();
      metrics->activeSampleDepthsProcessed = 0;
      metrics->radianceDeltaSquaredSumPerDepth.clear();
      metrics->maxRadianceDeltaPerDepth.clear();
      metrics->compatibilityShadeSamples = 0;
      metrics->stoppedByConvergence = false;
      metrics->stoppedAfterDepth = 0;
    }
    const bool trackRadianceDelta = metrics || settings.convergenceEnabled;

    for (int bounce = 0; bounce < m_maximumRecursionDepth; ++bounce) {
      const std::uint64_t activeCount = activePathCount();
      if (activeCount == 0) {
        break;
      }
      if (metrics) {
        metrics->activeSamplesPerDepth.push_back(activeCount);
        metrics->activeSampleDepthsProcessed += activeCount;
      }

      double depthDeltaSquaredSum = 0.0;
      double depthMaxDelta = 0.0;
      for (auto& path : paths) {
        if (!path.active) {
          continue;
        }

        const Colord accumulatedBeforeDepth = path.accumulated;
        const auto recordDepthDelta = [&] {
          if (!trackRadianceDelta) {
            return;
          }
          const double deltaSquared =
            radianceDeltaSquared(accumulatedBeforeDepth, path.accumulated);
          depthDeltaSquaredSum += deltaSquared;
          if (metrics) {
            depthMaxDelta = std::max(depthMaxDelta, std::sqrt(deltaSquared));
          }
        };

        if (isCancelled()) {
          path.active = false;
          recordDepthDelta();
          continue;
        }

        path.state.recurseIn();

        HitPointInterval hitPoints;
        const Primitive* primitive = scene.intersect(path.ray, hitPoints, path.state);
        if (!primitive) {
          path.accumulated += path.throughput * scene.background();
          path.state.recurseOut();
          path.active = false;
          recordDepthDelta();
          continue;
        }

        const HitPoint hitPoint = hitPoints.minWithPositiveDistance();
        if (bounce == 0) {
          path.state.hitPoint = hitPoint;
        }

        const auto material = primitive->material();
        if (!material) {
          path.state.recurseOut();
          path.active = false;
          recordDepthDelta();
          continue;
        }

        const Vector3d wi = -path.ray.direction().normalized();
        if (!material->supportsBsdfSampling()) {
          const Colord whittedColor =
            material->shade(&recursiveRayCaster, scene, path.ray, hitPoint, path.state);
          path.accumulated += path.throughput * whittedColor;
          if (metrics) {
            ++metrics->compatibilityShadeSamples;
          }
          path.state.recurseOut();
          path.active = false;
          recordDepthDelta();
          continue;
        }

        for (const auto& light : scene.lights()) {
          path.accumulated +=
            path.throughput * directLighting(scene, *light, hitPoint, *material, wi, path.state);
        }

        const Vector2d bsdfSample = path.state.sampleStream->sample2D(
          SampleDimension::BSDF, static_cast<std::uint64_t>(bounce));
        const MaterialBsdfSample sampled = material->sampleBsdf(hitPoint, wi, bsdfSample);
        if (sampled.pdf <= 0.0 || sampled.value == Colord::black()) {
          path.state.recurseOut();
          path.active = false;
          recordDepthDelta();
          continue;
        }

        const double normalDotWo = hitPoint.normal() * sampled.direction;
        if (!sampled.isDelta && normalDotWo <= 0.0) {
          path.state.recurseOut();
          path.active = false;
          recordDepthDelta();
          continue;
        }

        if (sampled.isDelta) {
          path.throughput = path.throughput * sampled.value;
        } else {
          path.throughput = path.throughput * (sampled.value * (normalDotWo / sampled.pdf));
        }

        if (bounce >= m_russianRouletteDepth) {
          const double survival = std::clamp(
            std::max({path.throughput.r(), path.throughput.g(), path.throughput.b()}), 0.05, 0.95);
          const double roulette = path.state.sampleStream->sample1D(
            SampleDimension::Continuation, static_cast<std::uint64_t>(bounce));
          if (roulette >= survival) {
            path.state.recurseOut();
            path.active = false;
            recordDepthDelta();
            continue;
          }
          path.throughput = path.throughput * (1.0 / survival);
        }

        path.ray = sampled.rayFrom(hitPoint);
        path.state.recurseOut();
        recordDepthDelta();
      }

      if (metrics) {
        metrics->radianceDeltaSquaredSumPerDepth.push_back(depthDeltaSquaredSum);
        metrics->maxRadianceDeltaPerDepth.push_back(depthMaxDelta);
      }

      if (settings.progressObserver) {
        std::vector<Colord> snapshot;
        snapshot.reserve(paths.size());
        for (const auto& path : paths) {
          snapshot.push_back(path.accumulated);
        }
        settings.progressObserver->depthCompleted(static_cast<std::uint64_t>(bounce + 1), snapshot,
                                                  activePathCount());
      }

      if (settings.convergenceEnabled && !paths.empty()) {
        const double activeFraction =
          static_cast<double>(activePathCount()) / static_cast<double>(paths.size());
        const double radianceDeltaRms =
          activeCount == 0 ? 0.0
                           : std::sqrt(depthDeltaSquaredSum / static_cast<double>(activeCount));
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

    std::vector<Colord> result;
    result.reserve(paths.size());
    for (const auto& path : paths) {
      result.push_back(path.accumulated);
    }
    return result;
  }
}
