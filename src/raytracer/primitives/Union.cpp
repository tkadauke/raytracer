#include "raytracer/primitives/Union.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

using namespace raytracer;

Primitive* Union::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    if (i->intersect(ray, candidate)) {
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

bool Union::intersects(const Ray& ray) {
  for (const auto& i : primitives()) {
    if (i->intersects(ray)) {
      return true;
    }
  }
  return false;
}
