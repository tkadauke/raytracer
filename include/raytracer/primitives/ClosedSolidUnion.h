#pragma once

#include "raytracer/primitives/Composite.h"

namespace raytracer {
  class ClosedSolidUnion : public Composite {
  public:
    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const;
    virtual Vector3d farthestPoint(const Vector3d& direction) const;
  };
}
