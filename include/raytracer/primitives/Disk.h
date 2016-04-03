#pragma once

#include "raytracer/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace raytracer {
  class Disk : public Primitive {
  public:
    inline explicit Disk(const Vector3d& center, const Vector3d& normal, double radius)
      : Primitive(),
        m_center(center),
        m_normal(normal),
        m_radius(radius),
        m_squaredRadius(radius * radius)
    {
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const;
    virtual Vector3d farthestPoint(const Vector3d& direction) const;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const;

  private:
    Vector4d m_center;
    Vector3d m_normal;
    double m_radius, m_squaredRadius;
  };
}
