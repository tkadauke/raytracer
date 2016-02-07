#include "raytracer/primitives/Union.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

using namespace raytracer;

Primitive* Union::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  bool hit = false;
  
  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    if (i->intersect(ray, candidate)) {
      hit = true;
      hitPoints = hitPoints | candidate;
    }
  }
  
  return hit ? this : 0;
}

bool Union::intersects(const Ray& ray) {
  for (const auto& i : primitives()) {
    if (i->intersects(ray)) {
      return true;
    }
  }
  return false;
}
