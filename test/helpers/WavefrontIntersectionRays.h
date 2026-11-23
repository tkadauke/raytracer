#pragma once

#include "core/math/Ray.h"
#include "core/math/Vector.h"

#include <cstddef>
#include <random>
#include <vector>

namespace test::helpers {
  /**
   * Deterministic ray fan used by the wavefront intersection benchmarks and
   * performance-regression tests: origins sit in front of the mesh-heavy
   * scene and aim at jittered targets behind it.
   */
  inline std::vector<Rayd> wavefrontIntersectionRays(std::size_t count) {
    std::mt19937 rng(1234);
    std::uniform_real_distribution<double> xy(-8.0, 8.0);
    std::uniform_real_distribution<double> z(3.5, 22.5);
    std::uniform_real_distribution<double> jitter(-0.25, 0.25);

    std::vector<Rayd> rays;
    rays.reserve(count);
    for (std::size_t index = 0; index != count; ++index) {
      const Vector3d origin(xy(rng), 2.5 + jitter(rng), -5.0);
      const Vector3d target(xy(rng), jitter(rng), z(rng));
      rays.emplace_back(origin, (target - origin).normalized());
    }
    return rays;
  }
}
