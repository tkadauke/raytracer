#include "raytracer/State.h"
#include "raytracer/primitives/ClosedSolidUnion.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

using namespace raytracer;

const Primitive* ClosedSolidUnion::intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return nullptr;
  }

  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    i->intersect(ray, candidate, state);
    // Add hitpoints regardless of the result above. We also want to know about
    // objects that we have hit behind the origin, so we can correctly build
    // complex CSG models. This is especially important 
    hitPoints = hitPoints + candidate;
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

Vector3d ClosedSolidUnion::farthestPoint(const Vector3d& direction) const {
  for (const auto& primitive : primitives()) {
    Vector3d point = primitive->farthestPoint(direction);
    if (!point.isUndefined())
      return point;
  }
  return Vector3d::undefined();
}
