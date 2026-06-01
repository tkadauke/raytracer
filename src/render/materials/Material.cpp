#include "render/materials/Material.h"

#include "core/math/HitPoint.h"

namespace render {
  Rayd MaterialBsdfSample::rayFrom(const HitPoint& hitPoint) const {
    if (continuationRay) {
      return *continuationRay;
    }
    return Rayd(hitPoint.point(), direction).epsilonShifted();
  }
}
