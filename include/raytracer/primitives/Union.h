#ifndef UNION_H
#define UNION_H

#include "raytracer/primitives/Composite.h"

namespace raytracer {
  class Union : public Composite {
  public:
    virtual Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints);
    virtual bool intersects(const Ray& ray);
  };
}

#endif
