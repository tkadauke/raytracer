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

Primitive* Composite::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  Primitive* hit = nullptr;
  double minDistance = numeric_limits<double>::infinity();
  
  for (const auto& i : m_primitives) {
    HitPointInterval candidate;
    auto primitive = i->intersect(ray, candidate);
    if (primitive) {
      double distance = candidate.min().distance();
      if (distance < minDistance) {
        hit = primitive;
        minDistance = distance;
        hitPoints = candidate;
      }
    }
  }
  
  return hit;
}

bool Composite::intersects(const Ray& ray) {
  for (const auto& i : m_primitives) {
    if (i->intersects(ray))
      return true;
  }
  
  return false;
}
