#include "render/WhittedIntegrator.h"

#include "core/ScopeExit.h"
#include "core/math/Constants.h"
#include "core/math/HitPoint.h"
#include "core/math/HitPointInterval.h"
#include "render/RayCaster.h"
#include "render/State.h"
#include "render/materials/Material.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"

#include <algorithm>
#include <cmath>
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

  std::vector<Colord> WhittedIntegrator::radianceBatch(
    const Scene& scene, const std::vector<IntegratorRaySample>& samples,
    const RayCaster& recursiveRayCaster, IntegratorBatchMetrics* metrics,
    const IntegratorBatchSettings& settings) const {
    struct QueuedRay {
      std::size_t sampleIndex{0};
      Rayd ray;
      Colord weight{Colord::white()};
      State state;
    };

    if (metrics) {
      metrics->usedScalarFallback = false;
      metrics->activeSamplesPerDepth.clear();
      metrics->radianceDeltaSquaredSumPerDepth.clear();
      metrics->maxRadianceDeltaPerDepth.clear();
      metrics->compatibilityShadeSamples = 0;
      metrics->stoppedByConvergence = false;
      metrics->stoppedAfterDepth = 0;
    }

    std::vector<Colord> result(samples.size(), Colord::black());
    const bool trackRadianceDelta = metrics || settings.convergenceEnabled;
    std::vector<QueuedRay> current;
    current.reserve(samples.size());
    for (std::size_t index = 0; index != samples.size(); ++index) {
      State state;
      state.timeSample = samples[index].timeSample;
      state.sampleStream = samples[index].sampleStream.get();
      current.push_back(QueuedRay{index, samples[index].ray, Colord::white(), std::move(state)});
    }

    for (int depth = 0; depth != m_maximumRecursionDepth && !current.empty(); ++depth) {
      if (metrics) {
        metrics->activeSamplesPerDepth.push_back(current.size());
      }

      std::vector<QueuedRay> next;
      double depthDeltaSquaredSum = 0.0;
      double depthMaxDelta = 0.0;

      for (auto& queued : current) {
        const Colord sampleBeforeDepth =
          trackRadianceDelta ? result[queued.sampleIndex] : Colord::black();
        const auto recordDepthDelta = [&] {
          if (!trackRadianceDelta) {
            return;
          }
          const double deltaSquared =
            radianceDeltaSquared(sampleBeforeDepth, result[queued.sampleIndex]);
          depthDeltaSquaredSum += deltaSquared;
          if (metrics) {
            depthMaxDelta = std::max(depthMaxDelta, std::sqrt(deltaSquared));
          }
        };

        if (isCancelled()) {
          result[queued.sampleIndex] += queued.weight * scene.background();
          recordDepthDelta();
          continue;
        }

        queued.state.recurseIn();
        ScopeExit recurseOut([&] { queued.state.recurseOut(); });

        if (queued.state.recursionDepth == m_maximumRecursionDepth) {
          queued.state.recordEvent(
            nullptr, "Raytracer: maximum recursion depth reached, returning background");
          result[queued.sampleIndex] += queued.weight * scene.background();
          recordDepthDelta();
          continue;
        }

        if (queued.state.throughput < RAYTRACER_THROUGHPUT_CUTOFF) {
          queued.state.recordEvent(nullptr,
                                   "Raytracer: throughput below cutoff, returning background");
          result[queued.sampleIndex] += queued.weight * scene.background();
          recordDepthDelta();
          continue;
        }

        HitPointInterval hitPoints;
        const Primitive* primitive = scene.intersect(queued.ray, hitPoints, queued.state);
        if (isCancelled()) {
          result[queued.sampleIndex] += queued.weight * scene.background();
          recordDepthDelta();
          continue;
        }

        if (!primitive) {
          queued.state.recordEvent(nullptr, "Raytracer: Nothing hit, returning background color");
          result[queued.sampleIndex] += queued.weight * scene.background();
          recordDepthDelta();
          continue;
        }

        const HitPoint hitPoint = hitPoints.minWithPositiveDistance();
        if (queued.state.recursionDepth == 1) {
          queued.state.hitPoint = hitPoint;
        }

        const auto material = primitive->material();
        if (!material) {
          queued.state.recordEvent(nullptr, "Raytracer: no material found, returning black");
          recordDepthDelta();
          continue;
        }

        queued.state.recordEvent(nullptr, "Raytracer: shading material");
        if (!material->supportsWhittedContinuations()) {
          if (metrics) {
            metrics->usedScalarFallback = true;
            ++metrics->compatibilityShadeSamples;
          }
          result[queued.sampleIndex] +=
            queued.weight *
            material->shade(&recursiveRayCaster, scene, queued.ray, hitPoint, queued.state);
          recordDepthDelta();
          continue;
        }

        const WhittedShadeResult shaded =
          material->shadeWhitted(&recursiveRayCaster, scene, queued.ray, hitPoint, queued.state);
        result[queued.sampleIndex] += queued.weight * shaded.localRadiance;

        for (const auto& continuation : shaded.continuations) {
          next.push_back(QueuedRay{
            queued.sampleIndex,
            continuation.ray,
            queued.weight * continuation.weight,
            continuationState(queued.state, queued.state.throughput * continuation.throughputScale),
          });
        }

        recordDepthDelta();
      }

      if (metrics) {
        metrics->radianceDeltaSquaredSumPerDepth.push_back(depthDeltaSquaredSum);
        metrics->maxRadianceDeltaPerDepth.push_back(depthMaxDelta);
      }

      if (settings.progressObserver) {
        settings.progressObserver->depthCompleted(static_cast<std::uint64_t>(depth + 1), result,
                                                  next.size());
      }

      if (settings.convergenceEnabled && !samples.empty()) {
        const double activeFraction =
          static_cast<double>(next.size()) / static_cast<double>(samples.size());
        const double radianceDeltaRms =
          current.empty() ? 0.0
                          : std::sqrt(depthDeltaSquaredSum / static_cast<double>(current.size()));
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
