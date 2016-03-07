#pragma once

#include "raytracer/primitives/Composite.h"

namespace raytracer {
  class Difference : public Composite {
  public:
    virtual Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints);
    virtual bool intersects(const Ray& ray);
    virtual BoundingBoxd boundingBox();
  };
}
