#pragma once

#include "raytracer/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace raytracer {
  class Triangle : public Primitive {
  public:
    Triangle(const Vector3d& a, const Vector3d& b, const Vector3d& c)
      : Primitive(),
        m_point0(a),
        m_point1(b),
        m_point2(c)
    {
      m_normal = computeNormal();
    }

    virtual BoundingBoxd boundingBox();
    virtual Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state);

  private:
    Vector3d computeNormal() const;

    Vector3d m_point0, m_point1, m_point2;
    Vector3d m_normal;
  };
}
