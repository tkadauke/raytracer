#include "primitives/Primitive.h"
#include "math/Ray.h"
#include "math/HitPointInterval.h"

bool Primitive::intersects(const Ray& ray) {
  HitPointInterval hitPoints;
  return intersect(ray, hitPoints);
}
