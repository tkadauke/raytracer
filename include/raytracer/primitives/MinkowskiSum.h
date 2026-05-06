#pragma once

#include "raytracer/primitives/ConvexOperation.h"

namespace raytracer {
  class MinkowskiSum : public ConvexOperation {
  public:
    virtual Vector3d farthestPoint(const Vector3d& direction) const;

    /** CSG mesh booleans are queued under roadmap §4.2.a. Returns empty Mesh. */
    virtual std::shared_ptr<Mesh> tessellate(int lod) const;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const;
  };
}
