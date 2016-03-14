#include "raytracer/State.h"
#include "raytracer/primitives/Intersection.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

using namespace raytracer;

Primitive* Intersection::intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) {
  unsigned int numHits = 0;
  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    if (i->intersect(ray, candidate, state)) {
      if (numHits) {
        hitPoints = hitPoints & candidate;
      } else {
        hitPoints = candidate;
      }
      numHits++;
    }
  }
  
  if (numHits != primitives().size() || hitPoints.empty()) {
    return nullptr;
  } else {
    if (material()) {
      hitPoints.setPrimitive(this);
      return this;
    } else {
      return hitPoints.minWithPositiveDistance().primitive();
    }
  }
}

bool Intersection::intersects(const Rayd& ray) {
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
