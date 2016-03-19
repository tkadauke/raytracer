#include "raytracer/primitives/ClosedSolidUnion.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

using namespace raytracer;

Primitive* ClosedSolidUnion::intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) {
  if (!boundingBoxIntersects(ray)) {
    return nullptr;
  }

  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    if (i->intersect(ray, candidate, state)) {
      hitPoints = hitPoints + candidate;
    }
  }
  hitPoints = hitPoints.merged();
  
  auto hitPoint = hitPoints.minWithPositiveDistance();
  if (hitPoint.isUndefined()) {
    return nullptr;
  } else {
    if (material()) {
      hitPoints.setPrimitive(this);
      return this;
    } else {
      return hitPoint.primitive();
    }
  }
}
