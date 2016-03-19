#pragma once

#include "raytracer/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace raytracer {
  class Sphere : public Primitive {
  public:
    inline Sphere(const Vector3d& origin, double radius)
      : m_origin(origin),
        m_radius(radius)
    {
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const;
    virtual bool intersects(const Rayd& ray, State& state) const;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const;

  private:
    Vector3d m_origin;
    double m_radius;
  };
}
