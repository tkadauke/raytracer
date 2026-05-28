#pragma once

#include <list>
#include <memory>

#include "core/math/BoundingBox.h"
#include "core/math/Ray.h"

class HitPointInterval;

namespace render {
  class Primitive;
  class State;

  /**
    * @brief Explicit contract for primitive containers that can be
    *        built into a spatial acceleration structure.
    *
    * `Composite`, `BVH`, and `Grid` have historically shared this
    * shape implicitly through `Primitive` plus `Composite::add`.
    * This interface names that contract without changing the
    * default scene behavior: callers can build a flat fallback,
    * a BVH, or a uniform grid through the same small surface.
    */
  class SpatialIndex {
  public:
    using PrimitivePtr = std::shared_ptr<Primitive>;
    using Primitives = std::list<PrimitivePtr>;

    virtual ~SpatialIndex() = default;

    /**
      * Add a primitive to the index input set. Implementations that
      * need a build step consume this list in `setup()`.
      */
    virtual void add(PrimitivePtr primitive) = 0;

    /// @returns the primitive input set in insertion order.
    virtual const Primitives& primitives() const = 0;

    /**
      * Build or refresh acceleration data from the current input set.
      * Flat fallback containers may implement this as a no-op.
      */
    virtual void setup() = 0;

    /// @returns the outer bounds used by traversal and culling.
    virtual const BoundingBoxd& bounds() const = 0;

    /**
      * Full ray query. Appends hit intervals and returns the leaf
      * primitive that produced the closest hit, or null on miss.
      */
    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                       render::State& state) const = 0;

    /// Boolean shadow-style query that may short-circuit on any hit.
    virtual bool intersects(const Rayd& ray, render::State& state) const = 0;
  };
}
