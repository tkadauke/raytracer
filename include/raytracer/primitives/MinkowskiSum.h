#pragma once

#include "raytracer/primitives/ConvexOperation.h"

namespace raytracer {
  class MinkowskiSum : public ConvexOperation {
  public:
    virtual Vector3d farthestPoint(const Vector3d& direction) const;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const;
  };
}
