#pragma once

#include "raytracer/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace raytracer {
  class Box : public Primitive {
  public:
    inline explicit Box(const Vector3d& center, const Vector3d& edge)
      : m_center(center),
        m_edge(edge)
    {
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const;
    
    /**
      * @returns the farthest point on the box in the given diretion. The
      *   following interactive figure illustrates the geometry. Click and drag
      *   horizontally to change the angle of the direction vector. The
      *   resulting point is highlighted in red.
      * 
      * @htmlonly
      * <script type="text/javascript" src="figure.js"></script>
      * <script type="text/javascript" src="box_farthest_point.js"></script>
      * @endhtmlonly
      */
    virtual Vector3d farthestPoint(const Vector3d& direction) const;
  
  protected:
    virtual BoundingBoxd calculateBoundingBox() const;

  private:
    Vector3d m_center, m_edge;
  };
}
