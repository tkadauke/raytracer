#pragma once

#include "render/primitives/ConvexOperation.h"

namespace render {
  /**
    * Convex CSG primitive whose support point is the sum of each
    * child's support point in the same direction. The related
    * intersection test uses the Minkowski difference: a support point
    * of \f$A - B\f$ is `supportA(v) - supportB(-v)`. GJK samples those
    * points to build a simplex that moves toward the origin.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="support_mapping_gjk.js"></script>
    * @endhtmlonly
    */
  class MinkowskiSum : public ConvexOperation {
  public:
    /**
      * @returns the sum of all child support points in `direction`.
      * This is the support function for the Minkowski sum itself.
      */
    virtual Vector3d farthestPoint(const Vector3d& direction) const override;

    /** CSG mesh booleans are not implemented. Returns empty Mesh. */
    virtual std::shared_ptr<Mesh> tessellate(int lod) const override;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const override;
  };
}
