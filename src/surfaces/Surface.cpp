#include "surfaces/Surface.h"
#include "math/Ray.h"
#include "math/HitPointInterval.h"

bool Surface::intersects(const Ray& ray) {
  HitPointInterval hitPoints;
  return intersect(ray, hitPoints);
}
