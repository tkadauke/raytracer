#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"

class HitPoint;

namespace raytracer {
  class BRDF {
  public:
    inline Colord operator()(const HitPoint& hitPoint, const Vector3d& in, const Vector3d& out) {
      return calculate(hitPoint, in, out);
    }
    
    virtual Colord calculate(const HitPoint& hitPoint, const Vector3d& in, const Vector3d& out) = 0;
    virtual Colord reflectance(const HitPoint& hitPoint, const Vector3d& out) = 0;
  };
}
