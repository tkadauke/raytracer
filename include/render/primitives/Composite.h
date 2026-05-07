#pragma once

#include <list>
#include <list>
#include <memory>

#include "render/primitives/Primitive.h"

namespace render {
  /**
    * @brief N-way OR over a flat list of primitives.
    *
    * `Composite` is a `Primitive` that contains other primitives.
    * Its `intersect` walks the children, returns the closest hit,
    * and forwards material lookups so a hit on a child surfaces
    * with the right material rather than the composite's own.
    *
    * Two flavours of composite exist:
    *
    *  - **`Composite`** (this class) — linear scan over every child.
    *    Fine for handfuls of primitives; suitable for `Scene`
    *    (which derives from this) when the geometry count is small.
    *  - **`Grid`** — spatial-hashing acceleration. Same N-way-OR
    *    semantics, but the intersect walks only the cells the ray
    *    passes through. Use this for meshes and dense scenes.
    *
    * The bounding box is the union of every child's bounding box,
    * computed lazily. Composites with mutable child lists must
    * invalidate the cache by calling `setup` (Grid does this) or
    * by reconstructing the composite (the typical pattern; the
    * world-to-runtime conversion produces a fresh composite per
    * render).
    *
    * @see Grid — accelerated subclass.
    * @see Scene — scene-graph root; adds lights + ambient/background.
    */
  class Composite : public Primitive {
  public:
    typedef std::list<std::shared_ptr<Primitive>> Primitives;

    inline Composite() {}

    ~Composite();

    /**
      * Test `ray` against every child primitive and return the
      * closest hit. Returns the *child* that was hit (so material
      * resolution follows the actual surface), with `hitPoints`
      * containing every entry/exit point produced by the children.
      *
      * If this composite has its own non-null material, it acts as
      * a fallback for hit children that don't have their own —
      * useful for "all the boxes in this group are made of glass"
      * semantics.
      */
    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, raytracer::State& state) const;

    /**
      * Boolean shadow-ray check across every child. Short-circuits
      * on the first hit — cheaper than `intersect` because the
      * t-value is irrelevant for shadow visibility.
      */
    virtual bool intersects(const Rayd& ray, raytracer::State& state) const;

    /// Append `primitive` to the child list. Order doesn't matter
    /// for correctness, but front-loaded common-hit primitives can
    /// (slightly) help `intersects` short-circuit faster.
    inline void add(std::shared_ptr<Primitive> primitive) {
      m_primitives.push_back(primitive);
    }

    /// @returns the child list. Iteration order matches insertion
    /// order.
    inline const Primitives& primitives() const {
      return m_primitives;
    }

    /**
      * Tessellate every child and concatenate the resulting meshes
      * into one. Face indices are remapped per child so each face
      * correctly references vertices in the merged vertex buffer.
      * Empty meshes (returned by infinite primitives like `Plane` or
      * unimplemented CSG ops) are silently absorbed — they contribute
      * zero vertices and zero faces. `lod` is passed through to every
      * child unchanged, since composites have no inherent geometry to
      * subdivide.
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod = 0) const;

  protected:
    /**
      * @returns the union of every child's bounding box, or a
      * default-constructed (empty) box for an empty composite.
      * Cached by the `Primitive` base.
      */
    virtual BoundingBoxd calculateBoundingBox() const;

  private:
    Primitives m_primitives;
  };
}
