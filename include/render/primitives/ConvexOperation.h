#pragma once

#include "render/primitives/Composite.h"

namespace render {
  class ConvexOperation : public Composite {
  public:
    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, raytracer::State& state) const;
    virtual bool intersects(const Rayd& ray, raytracer::State& state) const;

    virtual Vector3d farthestPoint(const Vector3d& direction) const = 0;
  };
}
