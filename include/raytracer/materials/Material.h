#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"

class HitPoint;
class Ray;

namespace raytracer {
  class Raytracer;

  class Material {
  public:
    virtual ~Material() {}

    virtual Colord shade(std::shared_ptr<Raytracer> raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth) = 0;
  };
}
