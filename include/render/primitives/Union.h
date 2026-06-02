#pragma once

#include "render/primitives/Composite.h"

namespace render {
  /**
    * Composite primitive that returns the union of its children's ray-hit
    * intervals. The interval-set operation is illustrated in
    * HitPointInterval.
    *
    * @see HitPointInterval
    */
  class Union : public Composite {
  public:
    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                       render::State& state) const override;
    PrimitivePacketHit4 intersectPacketHits(const Ray4& rays,
                                            const PrimitivePacketState4& states) const override;
    PrimitivePacketHit8 intersectPacketHits(const Ray8& rays,
                                            const PrimitivePacketState8& states) const override;
    virtual bool intersects(const Rayd& ray, render::State& state) const override;

    /** CSG mesh booleans are not implemented. Returns empty Mesh. */
    virtual std::shared_ptr<Mesh> tessellate(int lod) const override;
  };
}
