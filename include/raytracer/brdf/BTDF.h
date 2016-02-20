#pragma once

#include "raytracer/brdf/BRDF.h"

class Ray;

namespace raytracer {
  class BTDF : BRDF {
  public:
    virtual bool totalInternalReflection(const Ray& ray, const HitPoint& hitPoint) = 0;
  };
}
