#pragma once

#include "render/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace render {
  class Rectangle : public Primitive {
  public:
    inline explicit Rectangle(const Vector3d& corner, const Vector3d& leg1, const Vector3d& leg2)
      : Primitive(),
        m_corner(corner),
        m_leg1(leg1),
        m_leg2(leg2),
        m_normal((leg1 ^ leg2).normalized())
    {
      m_squaredLength1 = m_leg1.squaredLength();
      m_squaredLength2 = m_leg2.squaredLength();
    }

    inline explicit Rectangle(const Vector3d& corner, const Vector3d& leg1, const Vector3d& leg2, const Vector3d& normal)
      : Rectangle(corner, leg1, leg2)
    {
      m_normal = normal;
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, raytracer::State& state) const;

    /**
      * Returns a Mesh with 4 vertices and 2 triangles covering the rectangle.
      * The normal is the rectangle's plane normal. UVs span [0, 1]² with the
      * corner at (0,0), corner+leg1 at (1,0), corner+leg1+leg2 at (1,1), and
      * corner+leg2 at (0,1). The @p lod parameter is ignored.
      *
      * @image html rectangle_wireframe.png "Rectangle rendered through WireframeEngine"
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod) const;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const;

  private:
    Vector4d m_corner;
    Vector3d m_leg1, m_leg2, m_normal;
    double m_squaredLength1, m_squaredLength2;
  };
}
