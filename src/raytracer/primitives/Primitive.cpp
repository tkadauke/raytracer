#include "raytracer/primitives/Primitive.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

using namespace raytracer;

bool Primitive::intersects(const Ray& ray) {
  HitPointInterval hitPoints;
  return intersect(ray, hitPoints);
}
