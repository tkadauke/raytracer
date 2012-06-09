#include "surfaces/Difference.h"
#include "math/HitPointInterval.h"
#include "math/Ray.h"

Surface* Difference::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  bool firstElement = true;
  
  for (Surfaces::const_iterator i = surfaces().begin(); i != surfaces().end(); ++i) {
    HitPointInterval candidate;
    if ((*i)->intersect(ray, candidate)) {
      if (firstElement) {
        hitPoints = candidate;
      } else {
        hitPoints = hitPoints - candidate;
      }
    } else if (firstElement) {
      return 0;
    }
    firstElement = false;
  }
  
  return hitPoints.empty() || hitPoints.minWithPositiveDistance().isUndefined() ? 0 : this;
}

bool Difference::intersects(const Ray& ray) {
  HitPointInterval hitPoints;
  return intersect(ray, hitPoints) != 0;
}
