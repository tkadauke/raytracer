#pragma once

#include "core/math/Constants.h"
#include "core/math/Vector.h"

#include <cmath>

namespace render::detail {
  // Concentric mapping from a unit square sample to the unit disc (Shirley
  // 1997, "A Low Distortion Map Between Disk and Square"). Takes a 2D
  // sample in [0, 1]² and returns a uniformly distributed point on the
  // unit disc. The mapping is bijective and low-distortion, so
  // stratification of the input square carries over to the output disc.
  inline Vector2d concentricMapToDisc(const Vector2d& sample) {
    const double a = 2.0 * sample.x() - 1.0;
    const double b = 2.0 * sample.y() - 1.0;

    if (a == 0.0 && b == 0.0) {
      return Vector2d(0.0, 0.0);
    }

    double r = 0.0;
    double phi = 0.0;
    if (a * a > b * b) {
      r = a;
      phi = PI_OVER_4 * (b / a);
    } else {
      r = b;
      phi = PI_OVER_2 - PI_OVER_4 * (a / b);
    }
    return Vector2d(r * std::cos(phi), r * std::sin(phi));
  }
}
