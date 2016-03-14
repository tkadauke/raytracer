#include "raytracer/State.h"
#include "raytracer/primitives/Composite.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

#include <limits>

using namespace std;
using namespace raytracer;

Composite::~Composite() {
}

BoundingBoxd Composite::boundingBox() {
  BoundingBoxd b;
  for (const auto& i : m_primitives)
    b.include(i->boundingBox());
  return b;
}

Primitive* Composite::intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) {
  Primitive* hit = nullptr;
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

bool Composite::intersects(const Rayd& ray) {
  for (const auto& i : m_primitives) {
    if (i->intersects(ray))
      return true;
  }
  
  return false;
}
