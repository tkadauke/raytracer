#include "render/primitives/Scene.h"

#include "core/math/HitPointInterval.h"
#include "render/State.h"

#include <cmath>

namespace render {
  bool Scene::occludes(const Rayd& ray, State& state, double maxDistance) const {
    if (std::isinf(maxDistance)) {
      return intersects(ray, state);
    }

    if (maxDistance <= 0.0) {
      return false;
    }

    HitPointInterval hitPoints;
    if (!intersect(ray, hitPoints, state)) {
      return false;
    }

    const HitPoint hitPoint = hitPoints.minWithPositiveDistance();
    return !hitPoint.isUndefined() && hitPoint.distance() < maxDistance;
  }
}
