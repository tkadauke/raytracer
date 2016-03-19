#pragma once

#include "raytracer/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace raytracer {
  class Plane : public Primitive {
  public:
    Plane(const Vector3d& normal, double distance)
      : m_normal(normal),
        m_distance(distance)
    {
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const;
    virtual bool intersects(const Rayd& ray, State& state) const;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const;

  private:
    double calculateIntersectionDistance(const Rayd& ray) const;

    Vector3d m_normal;
    double m_distance;
  };
}
