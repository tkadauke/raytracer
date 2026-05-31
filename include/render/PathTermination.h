#pragma once

#include <algorithm>

#include "core/Color.h"

namespace render {
  inline constexpr double RAYTRACER_MIN_CONTINUATION_PROBABILITY = 0.05;

  /**
    * Result of a Russian-roulette path-continuation decision.
    *
    * When `continues` is true, callers must multiply the path throughput by
    * `weightScale` so the expected throughput remains unchanged:
    *
    * \f$E[T'] = p * (T / p) + (1 - p) * 0 = T\f$.
    */
  struct PathContinuation {
    bool continues;
    double probability;
    double weightScale;
  };

  /**
    * Continuation probability from scalar path throughput.
    *
    * Throughput in `(0, 1)` maps directly to probability, low non-zero
    * throughput is clamped to `minimumProbability` to avoid very large
    * survival weights, and throughput at/above one always continues.
    */
  [[nodiscard]] inline double continuationProbability(
    double throughput,
    double minimumProbability = RAYTRACER_MIN_CONTINUATION_PROBABILITY) noexcept {
    if (throughput <= 0.0) {
      return 0.0;
    }
    const double clampedMinimum = std::clamp(minimumProbability, 0.0, 1.0);
    return std::clamp(throughput, clampedMinimum, 1.0);
  }

  /**
    * Continuation probability from RGB path throughput. The maximum component
    * is the standard conservative scalar proxy: paths carrying high energy in
    * any channel are more likely to survive.
    */
  [[nodiscard]] inline double continuationProbability(
    const Colord& throughput,
    double minimumProbability = RAYTRACER_MIN_CONTINUATION_PROBABILITY) noexcept {
    return continuationProbability(throughput.max(), minimumProbability);
  }

  /**
    * Make a Russian-roulette continuation decision from an explicit random
    * sample in `[0, 1)`. The sample is passed in rather than pulled internally
    * so future integrators can reserve deterministic sampler dimensions.
    */
  [[nodiscard]] inline PathContinuation
  pathContinuation(double throughput, double sample,
                   double minimumProbability = RAYTRACER_MIN_CONTINUATION_PROBABILITY) noexcept {
    const double probability = continuationProbability(throughput, minimumProbability);
    const bool continues = sample < probability;
    return {continues, probability, continues && probability > 0.0 ? 1.0 / probability : 0.0};
  }

  /**
    * RGB-throughput overload for `pathContinuation`.
    */
  [[nodiscard]] inline PathContinuation
  pathContinuation(const Colord& throughput, double sample,
                   double minimumProbability = RAYTRACER_MIN_CONTINUATION_PROBABILITY) noexcept {
    return pathContinuation(throughput.max(), sample, minimumProbability);
  }

  /**
    * Apply the survival weight to scalar throughput. Terminated paths carry
    * zero throughput.
    */
  [[nodiscard]] inline double continuedThroughput(double throughput,
                                                  const PathContinuation& continuation) noexcept {
    return continuation.continues ? throughput * continuation.weightScale : 0.0;
  }

  /**
    * Apply the survival weight to RGB throughput. Terminated paths carry black.
    */
  [[nodiscard]] inline Colord continuedThroughput(const Colord& throughput,
                                                  const PathContinuation& continuation) {
    return continuation.continues ? throughput * continuation.weightScale : Colord::black();
  }
}
