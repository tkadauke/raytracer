#include "raytracer/State.h"
#include "raytracer/primitives/Union.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

using namespace raytracer;

Primitive* Union::intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) {
  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    if (i->intersect(ray, candidate, state)) {
      hitPoints = hitPoints | candidate;
    }
  }
  
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

bool Union::intersects(const Rayd& ray, State& state) {
  for (const auto& i : primitives()) {
    if (i->intersects(ray, state)) {
      return true;
    }
  }
  return false;
}
