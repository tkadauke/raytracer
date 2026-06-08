#pragma once
#include <memory>

#include "render/primitives/Primitive.h"
#include "core/math/Matrix.h"
#include "core/math/Ray.h"

namespace render {
  /**
    * @brief Wraps a primitive in a 4×4 transformation matrix —
    *        translation, rotation, scale, shear, etc.
    *
    * An `Instance` lets the same underlying `Primitive` appear at
    * multiple positions / orientations / sizes without duplicating
    * its geometry. The classic use is rendering N copies of a
    * detailed mesh, but every world-side `Surface` also produces an
    * `Instance` from `toRaytracerPrimitive` so the editor's
    * position / rotation / scale Q_PROPERTYs translate into runtime
    * geometry.
    *
    * Implementation: at intersect time, the ray is transformed
    * *into the primitive's local coordinate space* (origin and
    * direction multiplied by the inverse of the instance's matrix),
    * fed to the wrapped primitive, and the resulting hit points
    * are transformed *back* to world space (via the regular point
    * matrix) — and the normals via the special inverse-transpose
    * matrix needed to keep normals perpendicular to the surface
    * under non-uniform scaling.
    *
    * The four-matrix dance (`m_pointMatrix`, `m_originMatrix`,
    * `m_directionMatrix`, `m_normalMatrix`) precomputes the
    * different forms each transform needs to avoid recomputing
    * inverses per ray.
    *
    * The interactive figure below shows the same contract in 2D:
    * the world ray is transformed into the wrapped primitive's local
    * space for intersection, while normals use the inverse-transpose
    * transform so they stay perpendicular to non-uniformly scaled
    * geometry.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="instance_transform_normals.js"></script>
    * @endhtmlonly
    *
    * @see TorusScene — the canonical use of `Instance` for
    *      orientation, demonstrating glass torus + 90° X-rotation.
    */
  class Instance : public Primitive {
  public:
    /**
      * Wrap `primitive` with the identity transform. Call
      * `setMatrix` to apply a non-trivial transform; until then
      * the instance behaves exactly like the wrapped primitive.
      */
    inline explicit Instance(std::shared_ptr<Primitive> primitive)
        : m_primitive(primitive),
          m_basePosition(Vector3d::null),
          m_baseRotation(Vector3d::null),
          m_baseScale(Vector3d::one),
          m_velocity(Vector3d::null) {
    }
    virtual ~Instance() {
    }

    /**
      * Transforms `ray` into the wrapped primitive's local space,
      * delegates the intersect, and transforms the resulting hit
      * points + normals back to world space. The returned
      * `Primitive*` is the wrapped primitive (or its hit child for
      * composites), not the instance itself, so material lookups
      * find the right surface.
      */
    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                       render::State& state) const override;
    PrimitivePacketHit4 intersectPacketHits(const Ray4& rays,
                                            const PrimitivePacketState4& states) const override;
    PrimitivePacketHit8 intersectPacketHits(const Ray8& rays,
                                            const PrimitivePacketState8& states) const override;
    PrimitivePacketInterval4
    intersectPacketIntervals(const Ray4& rays, const PrimitivePacketState4& states) const override;
    PrimitivePacketInterval8
    intersectPacketIntervals(const Ray8& rays, const PrimitivePacketState8& states) const override;

    /// Boolean variant — same transform-and-delegate pattern.
    virtual bool intersects(const Rayd& ray, render::State& state) const override;

    /// @returns the instance's own material if set (overrides the
    /// wrapped primitive's material), otherwise the wrapped
    /// primitive's. This is what makes `Surface::applyTransform`
    /// preserve material assignment on world-to-runtime conversion.
    virtual std::shared_ptr<render::Material> material() const override;

    /**
      * Set the world-to-instance transform matrix. Internally
      * precomputes the inverse for ray transformation, the
      * direction-only inverse for ray directions, and the inverse-
      * transpose for normal transformation under non-uniform
      * scaling.
      */
    void setMatrix(const Matrix4d& matrix);

    /**
      * Set a per-shutter-tick linear velocity for motion blur. The
      * instance is at its `setMatrix` position at `timeSample = 0`
      * and at `position + velocity` at `timeSample = 1`. Each
      * primary ray uses an independent stratified time sample drawn
      * from the renderer's sample stream — see `Camera::render` for
      * the dimension allocation.
      *
      * When explicit transform animation tracks are attached, those
      * `position`, `rotation`, and `scale` tracks define the base
      * transform sampled at the current frame plus `timeSample`.
      * Velocity remains a convenience shutter offset layered after
      * that base transform. A runtime `velocity` track, when present,
      * replaces this static value for the sampled frame.
      *
      * Defaults to the zero vector, in which case `intersect` takes
      * the fast static path and the renderer behaves exactly as it
      * did before motion blur was added. Rotation and scale
      * tracks are sampled through the explicit animation path; the
      * velocity convenience property is only linear translation.
      *
      * The interactive figure below shows the same primitive sampled
      * at several shutter times. Drag the velocity endpoint to change
      * the path, scrub the shutter-time slider to see the instantaneous
      * position, and switch between regular and stochastic sampling to
      * see why time is just another sample dimension: a non-zero
      * linear velocity turns one static primitive into many
      * time-offset intersection tests that are averaged into the
      * ghosted silhouette.
      *
      * @htmlonly
      * <script type="text/javascript" src="figure.js"></script>
      * <script type="text/javascript" src="motion_blur_time_sampling.js"></script>
      * @endhtmlonly
      */
    void setVelocity(const Vector3d& velocity);

    /// @returns the configured velocity. Zero means no motion blur.
    inline const Vector3d& velocity() const {
      return m_velocity;
    }

    /// @returns the primitive shared by this instance.
    inline const std::shared_ptr<Primitive>& primitive() const {
      return m_primitive;
    }

    /// Support function — transforms `direction` into local space,
    /// queries the wrapped primitive, transforms the result back.
    virtual Vector3d farthestPoint(const Vector3d& direction) const override;

    /**
      * Emits overlay curve segments from the wrapped primitive after applying
      * this instance's point transform to each endpoint.
      */
    void forEachCurveOverlaySegment(const CurveOverlaySegmentVisitor& visitor) const override;

    void forEachTransformedLeaf(std::shared_ptr<render::Material> inheritedMaterial,
                                const Matrix4d& pointMatrix, const Matrix3d& normalMatrix,
                                const TransformedLeafVisitor& visitor) const override;
    void forEachTransformedLeafInBounds(const BoundsFilter& boundsFilter,
                                        std::shared_ptr<render::Material> inheritedMaterial,
                                        const Matrix4d& pointMatrix, const Matrix3d& normalMatrix,
                                        const TransformedLeafVisitor& visitor) const override;
    void appendIntersectionSceneRecords(IntersectionSceneBuilder& builder,
                                        std::shared_ptr<render::Material> inheritedMaterial,
                                        const Matrix4d& pointMatrix,
                                        const Matrix3d& normalMatrix) const override;

    /**
     * Tessellates the wrapped primitive and applies the instance
      * transform to every vertex: points by the point matrix, normals
      * by the inverse-transpose normal matrix (then re-normalised so
      * non-uniform scale doesn't break unit-length). UVs pass through
      * unchanged — they're an intrinsic surface parameterisation, not
      * a world-space quantity.
      *
      * Only the `t = 0` configuration is captured. A time-aware
      * engine that wants per-frame meshes for motion blur has to call
      * `tessellate()` once per frame and translate the result by
      * `velocity * t`. `lod` is forwarded to the wrapped primitive.
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod = 0) const override;

  protected:
    /// @returns the wrapped primitive's bounding box, transformed
    /// by the instance matrix and re-axis-aligned. For animated
    /// instances (non-zero `velocity`), expanded by the motion
    /// vector so the bbox covers every position the primitive
    /// occupies during the shutter.
    virtual BoundingBoxd calculateBoundingBox() const override;

  private:
    inline Rayd instancedRay(const Rayd& ray) const {
      return Rayd(m_originMatrix * ray.origin(), m_directionMatrix * ray.direction());
    }

    struct TransformSample {
      Matrix4d pointMatrix;
      Matrix4d originMatrix;
      Matrix3d directionMatrix;
      Matrix3d normalMatrix;
      Vector3d velocity;
      bool animated{false};
    };

    [[nodiscard]] bool hasRuntimeTransformAnimation() const;
    [[nodiscard]] double animationSampleTime(const render::State& state) const;
    [[nodiscard]] double animationBaselineTime(const render::State& state) const;
    [[nodiscard]] Vector3d sampledVectorTrack(const char* propertyName, double time,
                                              const Vector3d& fallback) const;
    [[nodiscard]] Vector3d sampledPositionTrack(double time, double baselineTime) const;
    [[nodiscard]] TransformSample transformSample(const render::State& state) const;
    [[nodiscard]] TransformSample transformSampleAtTime(double time, double shutterTimeSample,
                                                        double baselineTime) const;

    PrimitivePacketHit4 intersectMovingRay4PacketHits(const Ray4& rays,
                                                      const PrimitivePacketState4& states) const;
    PrimitivePacketHit8 intersectMovingRay8PacketHits(const Ray8& rays,
                                                      const PrimitivePacketState8& states) const;
    PrimitivePacketInterval4
    intersectMovingRay4PacketIntervals(const Ray4& rays, const PrimitivePacketState4& states) const;
    PrimitivePacketInterval8
    intersectMovingRay8PacketIntervals(const Ray8& rays, const PrimitivePacketState8& states) const;

    std::shared_ptr<Primitive> m_primitive;
    Matrix4d m_pointMatrix;
    Matrix4d m_originMatrix;
    Matrix3d m_directionMatrix;
    Matrix3d m_normalMatrix;
    Vector3d m_basePosition;
    Vector3d m_baseRotation;
    Vector3d m_baseScale;
    Vector3d m_velocity;
  };
}
