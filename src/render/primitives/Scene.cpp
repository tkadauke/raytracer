#include "render/primitives/Scene.h"

#include "core/math/HitPointInterval.h"
#include "render/State.h"
#include "render/lights/Light.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace render {
  void Scene::addLight(std::shared_ptr<render::Light> light) {
    if (auto emitter = light->emitterPrimitive()) {
      add(std::move(emitter));
    }
    m_lights.push_back(std::move(light));
  }

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
    const double occlusionLimit = std::max(0.0, maxDistance - Rayd::epsilon * 4.0);
    return !hitPoint.isUndefined() && hitPoint.distance() < occlusionLimit;
  }
}
