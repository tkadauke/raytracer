#pragma once

#include "raytracer/primitives/Composite.h"

namespace raytracer {
  class ClosedSolidUnion : public Composite {
  public:
    virtual Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints);
  };
}
