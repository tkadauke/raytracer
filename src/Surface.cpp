#include "Surface.h"
#include "Ray.h"
#include "HitPointInterval.h"

bool Surface::intersects(const Ray& ray) {
  HitPointInterval hitPoints;
  return intersect(ray, hitPoints);
}
