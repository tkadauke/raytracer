#include "raytracer/primitives/Intersection.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

using namespace raytracer;

Primitive* Intersection::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  Primitive* hit = nullptr;
  double minDistance = std::numeric_limits<double>::infinity();
  
  unsigned int numHits = 0;
  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    auto primitive = i->intersect(ray, candidate);
    if (primitive) {
      if (numHits) {
        hitPoints = hitPoints & candidate;
        double dist = hitPoints.minWithPositiveDistance().distance();
        if (dist != minDistance) {
          hit = primitive;
          minDistance = dist;
        }
      } else {
        hitPoints = candidate;
        hit = primitive;
        minDistance = hitPoints.minWithPositiveDistance().distance();
      }
      numHits++;
    }
  }
  
  if (numHits != primitives().size() || hitPoints.empty()) {
    return nullptr;
  } else {
    return material() ? this : hit;
  }
}

bool Intersection::intersects(const Ray& ray) {
  for (const auto& i : primitives()) {
    if (!i->intersects(ray))
      return false;
  }
  
  return true;
}

BoundingBoxd Intersection::boundingBox() {
  BoundingBoxd result;
  int num = 0;
  for (const auto& i : primitives()) {
    if (num++ == 0)
      result = i->boundingBox();
    else
      result &= i->boundingBox();
  }
  return result;
}
