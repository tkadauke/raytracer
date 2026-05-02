#pragma once

#include "raytracer/primitives/Primitive.h"
#include "core/DivisionByZeroException.h"
#include "core/math/Vector.h"

namespace raytracer {
  class OpenCylinder : public Primitive {
  public:
    /**
      * Constructs an open cylinder with the given @p radius and @p height.
      *
      * @throws DivisionByZeroException if @p radius is zero — IEEE 754
      *   division by zero would otherwise silently leave m_invRadius as
      *   +Infinity, which propagates into surface normals and corrupts
      *   shading for every ray hitting the cylinder.
      */
    inline explicit OpenCylinder(double radius, double height)
      : m_radius(radius),
        m_halfHeight(height / 2.0),
        m_invRadius(radius == 0.0 ? 0.0 : 1.0 / radius)
    {
      if (radius == 0.0)
        throw DivisionByZeroException(__FILE__, __LINE__);
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const;
    virtual bool intersects(const Rayd& ray, State& state) const;
    virtual Vector3d farthestPoint(const Vector3d& direction) const;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const;

  private:
    double m_radius;
    double m_halfHeight;
    
    double m_invRadius;
  };
}
