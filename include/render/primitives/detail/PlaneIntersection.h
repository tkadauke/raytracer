#pragma once

#include "core/math/Ray.h"
#include "core/math/Vector.h"

#include <cmath>
#include <optional>

namespace render::detail {
  /**
    * Solves the ray/plane intersection parameter `t` for `ray` against the
    * plane through `pointOnPlane` with normal `normal`. Returns
    * std::nullopt when the ray is parallel to the plane (zero or
    * non-finite denominator); callers apply their own miss reasons and
    * further containment tests (radius, quad extents, ...).
    */
  inline std::optional<double> solvePlaneParameter(const Vector4d& pointOnPlane,
                                                    const Vector3d& normal, const Rayd& ray) {
    const double denominator = ray.direction() * normal;
    if (denominator == 0.0) {
      return std::nullopt;
    }

    const double t = (pointOnPlane - ray.origin()) * normal / denominator;
    if (!std::isfinite(t)) {
      return std::nullopt;
    }

    return t;
  }
}
