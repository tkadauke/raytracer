#ifndef INTERSECTION_H
#define INTERSECTION_H

#include "raytracer/primitives/Composite.h"

namespace raytracer {
  class Intersection : public Composite {
  public:
    virtual Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints);
    virtual bool intersects(const Ray& ray);
    virtual BoundingBox boundingBox();
  };
}

#endif
