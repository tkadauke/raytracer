#pragma once

#include "raytracer/primitives/Composite.h"

namespace raytracer {
  class Difference : public Composite {
  public:
    virtual Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints);
    virtual bool intersects(const Rayd& ray);
    virtual BoundingBoxd boundingBox();
  };
}
