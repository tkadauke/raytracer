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
  
  return (hitPoints.empty() || hitPoints.minWithPositiveDistance().isUndefined()) ? 0 : this;
}

// Shadow implementation of Composite, which generates spourious shadows of
// differential objects
bool Difference::intersects(const Ray& ray) {
  HitPointInterval hitPoints;
  return intersect(ray, hitPoints);
}
