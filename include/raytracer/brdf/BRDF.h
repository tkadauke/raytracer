#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"

class HitPoint;

namespace raytracer {
  class BRDF {
  public:
    inline Colord operator()(const HitPoint& hitPoint, const Vector3d& out, const Vector3d& in) {
      return calculate(hitPoint, in, out);
    }
    
    virtual Colord calculate(const HitPoint& hitPoint, const Vector3d& out, const Vector3d& in);
    virtual Colord reflectance(const HitPoint& hitPoint, const Vector3d& out);
    virtual Colord sample(const HitPoint& hitPoint, const Vector3d& out, Vector3d& in);
  };
}
