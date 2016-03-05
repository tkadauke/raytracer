#pragma once

#include "core/math/Vector.h"
#include "core/Color.h"

namespace raytracer {
  class Light {
  public:
    inline Light() {}

    inline virtual ~Light() {}

    virtual Vector3d direction(const Vector3d& point) const = 0;
    virtual Colord radiance() const = 0;
  };
}
