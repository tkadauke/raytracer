#include "raytracer/primitives/Difference.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

using namespace raytracer;

Primitive* Difference::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  Primitive* hit = nullptr;
  bool firstElement = true;
  double minDistance = std::numeric_limits<double>::infinity();
  
  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    auto primitive = i->intersect(ray, candidate);
    if (primitive) {
      if (firstElement) {
        hitPoints = candidate;
        hit = primitive;
        minDistance = hitPoints.minWithPositiveDistance().distance();
      } else {
        hitPoints = hitPoints - candidate;
        double dist = hitPoints.minWithPositiveDistance().distance();
        if (dist != minDistance) {
          hit = primitive;
          minDistance = dist;
        }
      }
    } else if (firstElement) {
      return nullptr;
    }
    firstElement = false;
  }
  
  if (hitPoints.empty() || hitPoints.minWithPositiveDistance().isUndefined()) {
    return nullptr;
  } else {
    return material() ? this : hit;
  }
}

// Shadow implementation of Composite, which generates spourious shadows of
// differential objects
bool Difference::intersects(const Ray& ray) {
  HitPointInterval hitPoints;
  return intersect(ray, hitPoints);
}

BoundingBoxd Difference::boundingBox() {
  if (primitives().size() > 0) {
    return primitives().front()->boundingBox();
  } else {
    return BoundingBoxd();
  }
}
