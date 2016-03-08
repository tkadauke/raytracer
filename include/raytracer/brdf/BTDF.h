#pragma once

#include "raytracer/brdf/BRDF.h"
#include "core/math/Ray.h"

namespace raytracer {
  class BTDF : BRDF {
  public:
    virtual bool totalInternalReflection(const Rayd& ray, const HitPoint& hitPoint) = 0;
  };
}
