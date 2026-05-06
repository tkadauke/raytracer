#pragma once
#include <memory>

#include "raytracer/primitives/Primitive.h"
#include "core/math/Matrix.h"
#include "core/math/Ray.h"

namespace raytracer {
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
      : m_primitive(primitive)
    {
    }
    virtual ~Instance() { }

    /**
      * Transforms `ray` into the wrapped primitive's local space,
      * delegates the intersect, and transforms the resulting hit
      * points + normals back to world space. The returned
      * `Primitive*` is the wrapped primitive (or its hit child for
      * composites), not the instance itself, so material lookups
      * find the right surface.
      */
    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const;

    /// Boolean variant — same transform-and-delegate pattern.
    virtual bool intersects(const Rayd& ray, State& state) const;

    /// @returns the instance's own material if set (overrides the
    /// wrapped primitive's material), otherwise the wrapped
    /// primitive's. This is what makes `Surface::applyTransform`
    /// preserve material assignment on world-to-runtime conversion.
    virtual std::shared_ptr<Material> material() const;

    /**
      * Set the world-to-instance transform matrix. Internally
      * precomputes the inverse for ray transformation, the
      * direction-only inverse for ray directions, and the inverse-
      * transpose for normal transformation under non-uniform
      * scaling.
      */
    void setMatrix(const Matrix4d& matrix);

    /// Support function — transforms `direction` into local space,
    /// queries the wrapped primitive, transforms the result back.
    virtual Vector3d farthestPoint(const Vector3d& direction) const;

  protected:
    /// @returns the wrapped primitive's bounding box, transformed
    /// by the instance matrix and re-axis-aligned.
    virtual BoundingBoxd calculateBoundingBox() const;

  private:
    inline Rayd instancedRay(const Rayd& ray) const {
      return Rayd(m_originMatrix * ray.origin(), m_directionMatrix * ray.direction());
    }

    std::shared_ptr<Primitive> m_primitive;
    Matrix4d m_pointMatrix;
    Matrix4d m_originMatrix;
    Matrix3d m_directionMatrix;
    Matrix3d m_normalMatrix;
  };
}
