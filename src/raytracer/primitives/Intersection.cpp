#include "raytracer/primitives/Intersection.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

using namespace raytracer;

Primitive* Intersection::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  unsigned int numHits = 0;
  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    if (i->intersect(ray, candidate)) {
      if (numHits) {
        hitPoints = hitPoints & candidate;
      } else {
        hitPoints = candidate;
      }
      numHits++;
    }
  }
  
  return numHits != primitives().size() || hitPoints.empty() ? 0 : this;
}

bool Intersection::intersects(const Ray& ray) {
  for (const auto& i : primitives()) {
    if (!i->intersects(ray))
      return false;
  }
  
  return true;
}

BoundingBox Intersection::boundingBox() {
  BoundingBox result;
  int num = 0;
  for (const auto& i : primitives()) {
    if (num++ == 0)
      result = i->boundingBox();
    else
      result &= i->boundingBox();
  }
  return result;
}
