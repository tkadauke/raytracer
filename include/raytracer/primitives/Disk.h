#ifndef RAYTRACER_DISK_H
#define RAYTRACER_DISK_H

#include "raytracer/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace raytracer {
  class Disk : public Primitive {
  public:
    Disk(const Vector3d& center, const Vector3d& normal, double radius)
      : Primitive(), m_center(center), m_normal(normal), m_radius(radius), m_squaredRadius(radius * radius)
    {
    }

    virtual Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints);
    virtual BoundingBox boundingBox();

  private:
    Vector4d m_center;
    Vector3d m_normal;
    double m_radius, m_squaredRadius;
  };
}

#endif
