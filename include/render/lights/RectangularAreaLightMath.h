#pragma once

#include "core/math/Vector.h"

namespace render {

  /// Solid-angle PDF for sampling a rectangular area light: the squared
  /// distance (or ray parameter `t` along the hit direction) over the
  /// light's foreshortened area, as seen from the shading point.
  inline double rectangularAreaLightSolidAnglePdf(double distance, double cosLight, double area) {
    return (distance * distance) / (cosLight * area);
  }

  /// Ray parameter `t` at which a ray from `point` along `direction` hits
  /// the infinite plane through `center` with normal `normal`, given the
  /// already-computed `normalDotDirection = normal * direction`.
  inline double rectangularAreaLightPlaneHitDistance(const Vector3d& center, const Vector3d& point,
                                                     const Vector3d& normal,
                                                     double normalDotDirection) {
    return ((center - point) * normal) / normalDotDirection;
  }

}
