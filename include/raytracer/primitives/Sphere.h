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

    virtual Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints);
    virtual bool intersects(const Ray& ray);

    virtual BoundingBox boundingBox();

  private:
    Vector3d m_origin;
    double m_radius;
  };
}
