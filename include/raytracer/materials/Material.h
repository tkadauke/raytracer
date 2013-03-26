#ifndef MATERIAL_H
#define MATERIAL_H

#include "core/Color.h"
#include "core/math/Vector.h"

class HitPoint;
class Ray;

namespace raytracer {
  class Raytracer;

  class Material {
  public:
    virtual ~Material() {}

    virtual Colord shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth) = 0;
  };
}

#endif
