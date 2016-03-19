#pragma once

#include "raytracer/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace raytracer {
  class Torus : public Primitive {
  public:
    Torus(double sweptRadius, double tubeRadius)
      : m_sweptRadius(sweptRadius),
        m_tubeRadius(tubeRadius)
    {
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const;

    virtual BoundingBoxd boundingBox() const;

  private:
    Vector3d computeNormal(const Vector3d& p) const;

    double m_sweptRadius;
    double m_tubeRadius;
  };
}
