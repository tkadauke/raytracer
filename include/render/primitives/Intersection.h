#pragma once

#include "render/primitives/Composite.h"

namespace render {
  /**
    * Composite primitive that keeps only the ray-hit intervals shared by all
    * children. The interval-set operation is illustrated in HitPointInterval.
    *
    * @see HitPointInterval
    */
  class Intersection : public Composite {
  public:
    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, render::State& state) const;
    virtual bool intersects(const Rayd& ray, render::State& state) const;

    /** CSG mesh booleans are not implemented. Returns empty Mesh. */
    virtual std::shared_ptr<Mesh> tessellate(int lod) const;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const;
  };
}
