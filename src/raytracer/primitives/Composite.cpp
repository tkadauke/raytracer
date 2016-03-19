#include "raytracer/State.h"
#include "raytracer/primitives/Composite.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

#include <limits>

using namespace std;
using namespace raytracer;

Composite::~Composite() {
}

const Primitive* Composite::intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return nullptr;
  }

  const Primitive* hit = nullptr;
  double minDistance = numeric_limits<double>::infinity();
  
  for (const auto& i : m_primitives) {
    HitPointInterval candidate;
    auto primitive = i->intersect(ray, candidate, state);
    if (primitive) {
      hitPoints = hitPoints + candidate;

      double distance = candidate.minWithPositiveDistance().distance();
      if (distance < minDistance) {
        hit = primitive;
        minDistance = distance;
      }
    }
  }
  
  return hit;
}

bool Composite::intersects(const Rayd& ray, State& state) const {
  if (!boundingBoxIntersects(ray)) {
    return false;
  }

  for (const auto& i : m_primitives) {
    if (i->intersects(ray, state))
      return true;
  }
  
  return false;
}

BoundingBoxd Composite::boundingBox() const {
  BoundingBoxd b;
  for (const auto& i : m_primitives)
    b.include(i->boundingBox());
  return b;
}
