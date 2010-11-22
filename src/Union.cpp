#include "Union.h"
#include "HitPointInterval.h"
#include "Ray.h"

Surface* Union::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  bool hit = false;
  
  for (Surfaces::const_iterator i = surfaces().begin(); i != surfaces().end(); ++i) {
    HitPointInterval candidate;
    if ((*i)->intersect(ray, candidate)) {
      hit = true;
      hitPoints = hitPoints | candidate;
    }
  }
  
  return hit ? this : 0;
}
