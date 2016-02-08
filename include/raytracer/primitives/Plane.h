#pragma once

#include "raytracer/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace raytracer {
  class Plane : public Primitive {
  public:
    Plane(const Vector3d& normal, double distance)
      : m_normal(normal), m_distance(distance) {}

    virtual Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints);
    virtual bool intersects(const Ray& ray);
    virtual BoundingBox boundingBox();

  private:
    double calculateIntersectionDistance(const Ray& ray);

    Vector3d m_normal;
    double m_distance;
  };
}
