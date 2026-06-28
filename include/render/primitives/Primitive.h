#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "core/Color.h"
#include "core/math/Matrix.h"
#include "core/math/BoundingBox.h"
#include "core/math/HitPoint.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/MemoizedValue.h"

#include "render/Object.h"

class Mesh;

namespace render {
  class IntersectionSceneBuilder;
  class Material;
  class Primitive;
  class State;

  template<typename Packet>
  class PrimitivePacketHit {
  public:
    bool hit(std::size_t lane) const {
      return m_primitives[lane] != nullptr;
    }

    const Primitive* primitive(std::size_t lane) const {
      return m_primitives[lane];
    }

    const HitPoint& hitPoint(std::size_t lane) const {
      return m_hitPoints[lane];
    }

    bool scalarFallback(std::size_t lane) const {
      return m_scalarFallbacks[lane];
    }

    void setHit(std::size_t lane, const Primitive* primitive, const HitPoint& hitPoint,
                bool scalarFallback = false) {
      m_primitives[lane] = primitive;
      m_hitPoints[lane] = hitPoint;
      m_scalarFallbacks[lane] = scalarFallback;
    }

    bool setHitIfCloser(std::size_t lane, const Primitive* primitive, const HitPoint& hitPoint,
                        bool scalarFallback = false) {
      if (!primitive || hitPoint.isUndefined()) {
        return false;
      }

      if (!hit(lane) || hitPoint.distance() < m_hitPoints[lane].distance()) {
        setHit(lane, primitive, hitPoint, scalarFallback);
        return true;
      }

      return false;
    }

  private:
    std::array<const Primitive*, Packet::lanes> m_primitives{};
    std::array<HitPoint, Packet::lanes> m_hitPoints{};
    std::array<bool, Packet::lanes> m_scalarFallbacks{};
  };
  using PrimitivePacketHit4 = PrimitivePacketHit<Ray4>;
  using PrimitivePacketHit8 = PrimitivePacketHit<Ray8>;
  using PrimitivePacketState4 = std::array<render::State*, Ray4::lanes>;
  using PrimitivePacketState8 = std::array<render::State*, Ray8::lanes>;

  template<typename Packet>
  class PrimitivePacketInterval {
  public:
    bool hit(std::size_t lane) const {
      return m_primitives[lane] != nullptr;
    }

    bool hasInterval(std::size_t lane) const {
      return !m_intervals[lane].empty();
    }

    const Primitive* primitive(std::size_t lane) const {
      return m_primitives[lane];
    }

    const HitPointInterval& interval(std::size_t lane) const {
      return m_intervals[lane];
    }

    bool scalarFallback(std::size_t lane) const {
      return m_scalarFallbacks[lane];
    }

    void setInterval(std::size_t lane, const Primitive* primitive, const HitPointInterval& interval,
                     bool scalarFallback = false) {
      m_primitives[lane] = primitive;
      m_intervals[lane] = interval;
      m_scalarFallbacks[lane] = scalarFallback;
    }

    PrimitivePacketHit<Packet> closestHits(const Primitive* overridePrimitive = nullptr) const {
      PrimitivePacketHit<Packet> result;
      for (std::size_t lane = 0; lane != Packet::lanes; ++lane) {
        HitPoint hitPoint = m_intervals[lane].minWithPositiveDistance();
        if (hitPoint.isUndefined()) {
          continue;
        }

        if (overridePrimitive) {
          hitPoint.setPrimitive(overridePrimitive);
          result.setHit(lane, overridePrimitive, hitPoint, m_scalarFallbacks[lane]);
          continue;
        }

        if (hitPoint.primitive()) {
          result.setHit(lane, hitPoint.primitive(), hitPoint, m_scalarFallbacks[lane]);
        }
      }
      return result;
    }

  private:
    std::array<const Primitive*, Packet::lanes> m_primitives{};
    std::array<HitPointInterval, Packet::lanes> m_intervals{};
    std::array<bool, Packet::lanes> m_scalarFallbacks{};
  };
  using PrimitivePacketInterval4 = PrimitivePacketInterval<Ray4>;
  using PrimitivePacketInterval8 = PrimitivePacketInterval<Ray8>;

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
  class Primitive : public render::Object {
  public:
    using LeafVisitor = std::function<void(const Primitive*, std::shared_ptr<render::Material>)>;
    using BoundsFilter = std::function<bool(const BoundingBoxd&)>;
    using CurveOverlaySegmentVisitor =
      std::function<void(const Vector3d&, const Vector3d&, const std::optional<Colord>&)>;
    struct TransformedLeaf {
      const Primitive* primitive;
      std::shared_ptr<render::Material> material;
      Matrix4d pointMatrix;
      Matrix3d normalMatrix;
      const Primitive* object{nullptr};

      [[nodiscard]] const Primitive* objectPrimitive() const;
      Vector3d transformPoint(const Vector3d& point) const;
      Vector3d transformNormal(const Vector3d& normal) const;
      BoundingBoxd boundingBox() const;
    };
    using TransformedLeafVisitor = std::function<void(const TransformedLeaf&)>;

    inline Primitive()
        : m_material(nullptr) {
    }
    virtual ~Primitive() {
    }

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
    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                       render::State& state) const = 0;

    /**
      * Packet intersection entry points for SIMD/block traversal.
      * The base implementation preserves correctness by extracting
      * each lane and calling the scalar `intersect`; primitives with
      * a cheaper SoA kernel override the matching packet width.
      */
    virtual RayPacketIntersection4 intersectPacket(const Ray4& rays, render::State& state) const;
    virtual RayPacketIntersection8 intersectPacket(const Ray8& rays, render::State& state) const;
    /**
      * Materialized packet intersections for wavefront/frontier traversal.
      * A null pointer in `states` marks that lane inactive; implementations
      * must leave that lane empty and skip all intersection/counter work for it.
      */
    virtual PrimitivePacketHit4 intersectPacketHits(const Ray4& rays,
                                                    const PrimitivePacketState4& states) const;
    virtual PrimitivePacketHit8 intersectPacketHits(const Ray8& rays,
                                                    const PrimitivePacketState8& states) const;
    virtual PrimitivePacketInterval4
    intersectPacketIntervals(const Ray4& rays, const PrimitivePacketState4& states) const;
    virtual PrimitivePacketInterval8
    intersectPacketIntervals(const Ray8& rays, const PrimitivePacketState8& states) const;

    /**
      * Boolean flavour for shadow rays — "is anything between the
      * surface and the light?" The default implementation runs the
      * full `intersect` and discards `hitPoints`; convex primitives
      * with cheaper closed-form tests (`Sphere`, `Plane`) can
      * override. Updates `state.shadowHit`/`shadowMiss`.
      */
    virtual bool intersects(const Rayd& ray, render::State& state) const;

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
    inline void setMaterial(std::shared_ptr<render::Material> material) {
      m_material = material;
    }

    const std::string& renderTextureSubview() const {
      return m_renderTextureSubview;
    }

    void setRenderTextureSubview(std::string subviewName) {
      m_renderTextureSubview = std::move(subviewName);
    }

    /// @returns the material attached to this primitive, or null
    /// if none was set. See `setMaterial` for fallback semantics.
    inline virtual std::shared_ptr<render::Material> material() const {
      return m_material;
    }

    /**
      * Visit every leaf primitive under this node and pass the
      * material that will shade that leaf after composite fallback
      * inheritance is applied.
      */
    inline void forEachLeaf(const LeafVisitor& visitor) const {
      forEachLeaf(nullptr, visitor);
    }

    /**
      * Recursive worker for `forEachLeaf(visitor)`. Public so
      * composite implementations can continue traversal through
      * children held as `Primitive` pointers.
      */
    virtual void forEachLeaf(std::shared_ptr<render::Material> inheritedMaterial,
                             const LeafVisitor& visitor) const;

    /**
      * Visit leaves through a spatially grouped view of this primitive when
      * one is available. `boundsFilter` is called for bounded groups before
      * descending into their children; returning false rejects the whole group
      * before its leaves are flattened or tessellated. Primitives that do not
      * expose spatial grouping fall back to `forEachLeaf`, preserving the
      * existing traversal behavior.
      */
    inline void forEachLeafInBounds(const BoundsFilter& boundsFilter,
                                    const LeafVisitor& visitor) const {
      forEachLeafInBounds(boundsFilter, nullptr, visitor);
    }

    /**
      * Recursive worker for `forEachLeafInBounds(boundsFilter, visitor)`.
      * Composite nodes override this to make their child bounds visible to
      * rasterizer-style frustum culling without changing ray traversal.
      */
    virtual void forEachLeafInBounds(const BoundsFilter& boundsFilter,
                                     std::shared_ptr<render::Material> inheritedMaterial,
                                     const LeafVisitor& visitor) const;

    void forEachTransformedLeaf(const TransformedLeafVisitor& visitor) const;
    virtual void forEachTransformedLeaf(std::shared_ptr<render::Material> inheritedMaterial,
                                        const Matrix4d& pointMatrix, const Matrix3d& normalMatrix,
                                        const TransformedLeafVisitor& visitor) const;

    void forEachTransformedLeafInBounds(const BoundsFilter& boundsFilter,
                                        const TransformedLeafVisitor& visitor) const;
    virtual void forEachTransformedLeafInBounds(const BoundsFilter& boundsFilter,
                                                std::shared_ptr<render::Material> inheritedMaterial,
                                                const Matrix4d& pointMatrix,
                                                const Matrix3d& normalMatrix,
                                                const TransformedLeafVisitor& visitor) const;

    /**
      * Append this primitive's GPU-intersection representation to @p builder.
      * Leaf primitives that the compiled intersection scene supports override
      * this; the base implementation records an explicit unsupported reason.
      */
    virtual void appendIntersectionSceneRecord(IntersectionSceneBuilder& builder,
                                               const TransformedLeaf& leaf) const;
    virtual void appendIntersectionSceneRecords(IntersectionSceneBuilder& builder,
                                                std::shared_ptr<render::Material> inheritedMaterial,
                                                const Matrix4d& pointMatrix,
                                                const Matrix3d& normalMatrix,
                                                const Primitive* inheritedObject = nullptr) const;

    /**
      * @returns true when overlapping this primitive with another child inside
      * a ClosedSolidUnion requires interval CSG to avoid exposing internal
      * surfaces. Boundary-only pieces such as disks, rectangles, triangles,
      * and open-cylinder side walls override this to return false.
      */
    [[nodiscard]] virtual bool requiresClosedSolidUnionCsgWhenOverlapped() const {
      return true;
    }

    /**
      * Visit semantic curve segments for image-space/debug overlay rendering.
      *
      * Unlike `tessellate()`, this path exposes curves as their original center
      * lines and does not require a non-zero physical width. Composite and
      * instance nodes preserve grouping and transforms while leaf primitives
      * that are not curves simply contribute nothing.
      */
    virtual void forEachCurveOverlaySegment(const CurveOverlaySegmentVisitor& visitor) const;

    /**
      * Support function: returns the point on this primitive that
      * extends farthest in `direction`. Used by the CSG convex-hull
      * and Minkowski-sum primitives, which need a per-shape support
      * function to evaluate combined-shape intersections via GJK.
      * GJK repeatedly asks two convex shapes for support points in
      * opposite directions, subtracts them to form a point on the
      * Minkowski difference, and evolves a simplex toward the origin.
      * If that simplex encloses the origin, the convex shapes overlap.
      *
      * Default implementation scans the eight bounding-box corners,
      * which is correct for any primitive contained in its bounds.
      * Subclasses with cheap closed-form support functions (e.g.
      * `Sphere`: `center + radius * direction.normalized()`)
      * override for accuracy and speed.
      *
      * @htmlonly
      * <script type="text/javascript" src="figure.js"></script>
      * <script type="text/javascript" src="support_mapping_gjk.js"></script>
      * @endhtmlonly
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
      * ).
      *
      * The returned `shared_ptr<Mesh>` is fresh per call — callers
      * may modify it in place. Caching, if any, is the consumer's
      * responsibility.
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod = 0) const;

  protected:
    static PrimitivePacketState4 activePacketStates(const PrimitivePacketState4& states,
                                                    std::uint16_t activeMask);
    static PrimitivePacketState8 activePacketStates(const PrimitivePacketState8& states,
                                                    std::uint16_t activeMask);

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
    std::shared_ptr<render::Material> m_material;
    std::string m_renderTextureSubview;
    mutable MemoizedValue<BoundingBoxd> m_cachedBoundingBox;
  };
}
