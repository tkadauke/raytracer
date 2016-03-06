#include "raytracer/primitives/Union.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

using namespace raytracer;

Primitive* Union::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  Primitive* hit = nullptr;
  double minDistance = std::numeric_limits<double>::infinity();
  
  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    auto primitive = i->intersect(ray, candidate);
    if (primitive) {
      hitPoints = hitPoints | candidate;
      double dist = candidate.min().distance();
      if (dist < minDistance) {
        hit = primitive;
        minDistance = dist;
      }
    }
  }
  
  if (hitPoints.empty() || hitPoints.minWithPositiveDistance().isUndefined()) {
    return nullptr;
  } else {
    return material() ? this : hit;
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
