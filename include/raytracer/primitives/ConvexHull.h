#pragma once

#include "raytracer/primitives/ConvexOperation.h"

namespace raytracer {
  class ConvexHull : public ConvexOperation {
  public:
    /**
      * Calculates the farthest point of all the children, given the direction.
      * The method gets the indiviual farthest points from each child, projects
      * them onto a ray with the given direction (any ray is fine), and then
      * sorts the projected points by distance along the ray. Lastly, it chooses
      * the one that is farthest along the ray. The following interactive figure
      * illustratest the concept. Click and drag horizontally to change the
      * angle of the direction vector. The red dot shows the result of this
      * computation.
      *
      * @htmlonly
      * <script type="text/javascript" src="figure.js"></script>
      * <script type="text/javascript" src="convex_hull_farthest_point.js"></script>
      * @endhtmlonly
      */
    virtual Vector3d farthestPoint(const Vector3d& direction) const;
  };
}
