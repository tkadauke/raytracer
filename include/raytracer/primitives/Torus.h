#ifndef RAYTRACER_TORUS_H
#define RAYTRACER_TORUS_H

#include "raytracer/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace raytracer {
  class Torus : public Primitive {
  public:
    Torus(double sweptRadius, double tubeRadius)
      : m_sweptRadius(sweptRadius), m_tubeRadius(tubeRadius)
    {
    }

    virtual Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints);

    virtual BoundingBox boundingBox();

  private:
    Vector3d computeNormal(const Vector3d& p) const;

    double m_sweptRadius;
    double m_tubeRadius;
  };
}

#endif
