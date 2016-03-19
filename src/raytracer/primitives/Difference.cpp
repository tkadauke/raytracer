#include "raytracer/State.h"
#include "raytracer/primitives/Difference.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

using namespace raytracer;

const Primitive* Difference::intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return nullptr;
  }

  bool firstElement = true;
  
  for (const auto& i : primitives()) {
    HitPointInterval candidate;
    if (i->intersect(ray, candidate, state)) {
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

// Shadow implementation of Composite, which generates spourious shadows of
// differential objects
bool Difference::intersects(const Rayd& ray, State& state) const {
  HitPointInterval hitPoints;
  return intersect(ray, hitPoints, state);
}

BoundingBoxd Difference::calculateBoundingBox() const {
  if (primitives().size() > 0) {
    return primitives().front()->boundingBox();
  } else {
    return BoundingBoxd();
  }
}
