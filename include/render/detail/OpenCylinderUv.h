#pragma once

#include "core/math/Constants.h"

#include <cmath>
#include <utility>

namespace render::detail {
  /**
    * Computes the (u, v) texture coordinate for a point on the side of an
    * open cylinder whose axis runs along y, given the point's `x`/`z`
    * coordinates in the cylinder's local frame, its `y` coordinate, and the
    * cylinder's `halfHeight`. `u` wraps the angle around the axis via
    * `atan2` into [0, 1); `v` maps `y` linearly across the cylinder's
    * height, returning 0 for a degenerate (zero-height) cylinder.
    */
  template <typename T>
  std::pair<T, T> cylinderSideUv(T x, T z, T y, T halfHeight) {
    const T twoPi = static_cast<T>(TAU);
    T u = std::atan2(z, x) / twoPi;
    if (u < T(0)) {
      u += T(1);
    }

    const T height = T(2) * halfHeight;
    const T v = height == T(0) ? T(0) : (y + halfHeight) / height;
    return {u, v};
  }
}
