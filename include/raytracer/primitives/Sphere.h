#pragma once

#include "raytracer/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace raytracer {
  class Sphere : public Primitive {
  public:
    inline explicit Sphere(const Vector3d& origin, double radius)
      : m_origin(origin),
        m_radius(radius)
    {
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const;
    virtual bool intersects(const Rayd& ray, State& state) const;

    /**
      * Returns the farthest point on the sphere in the given diretion. The
      * following interactive figure illustrates the geometry. Click and drag
      * horizontally to change the angle of the direction vector. The resulting
      * point is highlighted in red.
      * 
      * @htmlonly
      * <script type="text/javascript" src="figure.js"></script>
      * <script type="text/javascript" src="sphere_farthest_point.js"></script>
      * @endhtmlonly
      */
    virtual Vector3d farthestPoint(const Vector3d& direction) const;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const;

  private:
    Vector3d m_origin;
    double m_radius;
  };
}
