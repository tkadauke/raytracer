#pragma once
#include <memory>

#include "core/math/BoundingBox.h"
#include "core/math/Ray.h"
#include "core/MemoizedValue.h"

#include "raytracer/Object.h"

class HitPointInterval;
class Mesh;

namespace raytracer {
  class Material;
  class State;

  /**
    * @brief Abstract base class for all geometric scene objects
    *        that can be intersected by a ray.
    *
    * Concrete subclasses include the leaf primitives (`Sphere`,
    * `Box`, `Plane`, `Torus`, `Cylinder`, `Mesh`, ...), the
    * compositors (`Composite`, `Grid`, `Instance`), and the CSG
    * combiners (`Difference`, `Union`, `Intersection`,
    * `MinkowskiSum`, `ConvexHull`). The abstract contract every
    * subclass implements is:
    *
    *  1. `intersect(ray, hitPoints, state)` — find every point at
    *     which `ray` enters or exits this primitive, push them into
    *     `hitPoints`, and return the leaf primitive that did the
    *     reporting (`this` for leaves; for composites, the actual
    *     hit child so material lookups land on the right surface).
    *  2. `calculateBoundingBox()` — return the axis-aligned bounding
    *     box. Cached lazily by `boundingBox()`.
    *
    * Anything else has a default in this base. `intersects` (the
    * boolean shadow-ray flavour) defaults to running `intersect`
    * and discarding the points; subclasses can override for cheaper
    * specialised tests. `farthestPoint` defaults to scanning the
    * eight bounding-box corners — sufficient for the convex-hull /
    * Minkowski-sum CSG operations to compute support points without
    * primitive-specific code.
    *
    * The lazy bounding-box cache is what makes spatial-acceleration
    * structures like `Grid` cheap to set up; recompute happens only
    * if the cache is explicitly reset (none of the leaf primitives
    * do this — all are static once constructed).
    *
    * @see Composite — N-way OR for collections of primitives.
    * @see Grid — spatial-hashing acceleration for large composites.
    * @see Instance — primitive wrapped in a transform matrix.
    */
  class Primitive : public Object {
  public:
    inline Primitive()
      : m_material(nullptr)
    {
    }
    virtual ~Primitive() {}

    /**
      * Test `ray` against the primitive and append every entry/exit
      * point to `hitPoints` in t-order. Returns the leaf primitive
      * that produced the hit (`this` for direct subclasses;
      * composites return the deepest matching child so material
      * resolution lands on the correct surface).
      *
      * State counters (`hit` / `miss`) are bumped via `state`.
      * Returning `nullptr` indicates a miss; `hitPoints` may still
      * be non-empty in that case if a wrapping composite has
      * partial information from siblings.
      */
    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const = 0;

    /**
      * Boolean flavour for shadow rays — "is anything between the
      * surface and the light?" The default implementation runs the
      * full `intersect` and discards `hitPoints`; convex primitives
      * with cheaper closed-form tests (`Sphere`, `Plane`) can
      * override. Updates `state.shadowHit`/`shadowMiss`.
      */
    virtual bool intersects(const Rayd& ray, State& state) const;

    /**
      * @returns the axis-aligned bounding box, computed lazily from
      * `calculateBoundingBox()` on first access and cached for
      * subsequent calls. Subclasses that mutate their geometry
      * after construction must invalidate the cache themselves.
      */
    inline const BoundingBoxd& boundingBox() const {
      if (!m_cachedBoundingBox) {
        m_cachedBoundingBox = calculateBoundingBox();
      }

      return m_cachedBoundingBox.value();
    }

    /**
      * Sets the material that will shade this primitive on hit.
      * Composites delegate to the hit child's material; if a
      * composite's `material()` returns non-null it acts as a
      * fallback for children with no material of their own.
      */
    inline void setMaterial(std::shared_ptr<Material> material) {
      m_material = material;
    }

    /// @returns the material attached to this primitive, or null
    /// if none was set. See `setMaterial` for fallback semantics.
    inline virtual std::shared_ptr<Material> material() const {
      return m_material;
    }

    /**
      * Support function: returns the point on this primitive that
      * extends farthest in `direction`. Used by the CSG convex-hull
      * and Minkowski-sum primitives, which need a per-shape support
      * function to evaluate combined-shape intersections via GJK.
      *
      * Default implementation scans the eight bounding-box corners,
      * which is correct for any primitive contained in its bounds.
      * Subclasses with cheap closed-form support functions (e.g.
      * `Sphere`: `center + radius * direction.normalized()`)
      * override for accuracy and speed.
      */
    virtual Vector3d farthestPoint(const Vector3d& direction) const;

    /**
      * Produce a triangle-mesh approximation of this primitive at
      * the requested level of detail. The output is a `core::Mesh`
      * with positions, normals, and UVs — the data type that
      * non-raytracing engines (wireframe, software raster, OpenGL
      * viewport) and exporters (OBJ, STL, glTF) consume.
      *
      * `lod` is an implementation-defined subdivision level, in the
      * spirit of OpenSubdiv:
      *
      *  - `lod = 0` → a minimum-reasonable tessellation. For Sphere
      *    that's a low-poly UV sphere; for Torus an 8×8 grid; for
      *    Box a fixed 12-triangle output (LOD ignored — Box is
      *    polyhedral already).
      *  - Higher values grow the subdivision count according to a
      *    per-primitive schedule documented on each override.
      *
      * The default implementation returns an empty `Mesh` and emits
      * a `state`-less `recordEvent`-style trace through stdout
      * `qWarning` so a debugging engine can see "primitive X did
      * not implement tessellate." Concrete leaf primitives override;
      * compositors (`Composite`, `Instance`, `Grid`, `Scene`)
      * concatenate or transform their children's meshes; CSG
      * primitives stub out until the mesh-boolean epic lands (see
      * roadmap §4.2.a).
      *
      * The returned `shared_ptr<Mesh>` is fresh per call — callers
      * may modify it in place. Caching, if any, is the consumer's
      * responsibility.
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod = 0) const;

  protected:
    /**
      * Compute the axis-aligned bounding box. Called lazily by
      * `boundingBox()` and cached. Must be a function of static
      * geometry only — this method should never read or write
      * mutable state outside the primitive's own constructor-fixed
      * fields.
      */
    virtual BoundingBoxd calculateBoundingBox() const = 0;

    /// Convenience wrapper: returns whether `ray` would hit this
    /// primitive's bounding box. Used by composites' `intersect`
    /// implementations as an early-out before recursing into
    /// children.
    inline bool boundingBoxIntersects(const Rayd& ray) const {
      return boundingBox().intersects(ray);
    }

    /**
      * Convex-shape intersection helper: assumes this primitive is
      * convex and uses the bounding-box-corner support function to
      * compute entry/exit points. Suitable for the convex CSG
      * operands (`Sphere`, `Box`, `Cylinder`); concave primitives
      * must implement `intersect` from scratch.
      */
    bool convexIntersect(const Rayd& ray, HitPointInterval& hitPoints) const;

  private:
    std::shared_ptr<Material> m_material;
    mutable MemoizedValue<BoundingBoxd> m_cachedBoundingBox;
  };
}
