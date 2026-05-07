#pragma once

#include "render/primitives/ConvexOperation.h"

namespace render {
  class MinkowskiSum : public ConvexOperation {
  public:
    virtual Vector3d farthestPoint(const Vector3d& direction) const;

    /** CSG mesh booleans are not implemented. Returns empty Mesh. */
    virtual std::shared_ptr<Mesh> tessellate(int lod) const;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const;
  };
}
