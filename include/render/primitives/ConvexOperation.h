#pragma once

#include "render/primitives/Composite.h"

namespace render {
  class ConvexOperation : public Composite {
  public:
    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                       render::State& state) const override;
    PrimitivePacketHit4 intersectPacketHits(const Ray4& rays,
                                            const PrimitivePacketState4& states) const override;
    virtual bool intersects(const Rayd& ray, render::State& state) const override;

    virtual Vector3d farthestPoint(const Vector3d& direction) const override = 0;
  };
}
