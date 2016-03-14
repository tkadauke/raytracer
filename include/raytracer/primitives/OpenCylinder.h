#pragma once

#include "raytracer/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace raytracer {
  class OpenCylinder : public Primitive {
  public:
    inline OpenCylinder(double radius, double height)
      : m_radius(radius),
        m_halfHeight(height / 2.0),
        m_invRadius(1.0 / radius)
    {
    }

    virtual Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state);
    virtual bool intersects(const Rayd& ray, State& state);
    virtual BoundingBoxd boundingBox();

  private:
    double m_radius;
    double m_halfHeight;
    
    double m_invRadius;
  };
}
