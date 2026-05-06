#pragma once

#include "raytracer/primitives/Composite.h"

namespace raytracer {
  class Union : public Composite {
  public:
    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const;
    virtual bool intersects(const Rayd& ray, State& state) const;

    /** CSG mesh booleans are queued under roadmap §4.2.a. Returns empty Mesh. */
    virtual std::shared_ptr<Mesh> tessellate(int lod) const;
  };
}
