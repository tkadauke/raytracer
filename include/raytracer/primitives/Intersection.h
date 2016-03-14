#pragma once

#include "raytracer/primitives/Composite.h"

namespace raytracer {
  class Intersection : public Composite {
  public:
    virtual Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state);
    virtual bool intersects(const Rayd& ray, State& state);
    virtual BoundingBoxd boundingBox();
  };
}
