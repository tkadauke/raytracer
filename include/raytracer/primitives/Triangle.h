#pragma once

#include "raytracer/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace raytracer {
  class Triangle : public Primitive {
  public:
    inline explicit Triangle(const Vector3d& a, const Vector3d& b, const Vector3d& c)
      : Primitive(),
        m_point0(a),
        m_point1(b),
        m_point2(c)
    {
      m_normal = computeNormal();
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const;

    /**
      * Returns a single-triangle Mesh identical to this Triangle. All three
      * vertices carry the triangle's flat normal. UV assignments follow the
      * standard barycentric convention: vertex 0 at (0,0), vertex 1 at (1,0),
      * vertex 2 at (0,1). The @p lod parameter is ignored.
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod) const;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const;

  private:
    Vector3d computeNormal() const;

    Vector3d m_point0, m_point1, m_point2;
    Vector3d m_normal;
  };
}
