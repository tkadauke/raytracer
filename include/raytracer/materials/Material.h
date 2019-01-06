#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"
#include "core/math/Ray.h"

#include "raytracer/Object.h"

class HitPoint;

namespace raytracer {
  class Raytracer;
  class State;

  class Material : public Object {
  public:
    virtual ~Material() {}

    virtual Colord shade(const Raytracer* raytracer, const Rayd& ray, const HitPoint& hitPoint, State& state) const = 0;
  };
}
