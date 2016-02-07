#include "raytracer/primitives/Difference.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

using namespace raytracer;

Primitive* Difference::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  bool firstElement = true;
  
  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    if (i->intersect(ray, candidate)) {
      if (firstElement) {
        hitPoints = candidate;
      } else {
        hitPoints = hitPoints - candidate;
      }
    } else if (firstElement) {
      return nullptr;
    }
    firstElement = false;
  }
  
  return hitPoints.empty() || hitPoints.minWithPositiveDistance().isUndefined() ? 0 : this;
}
