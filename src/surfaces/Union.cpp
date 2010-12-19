#include "surfaces/Union.h"
#include "math/HitPointInterval.h"
#include "math/Ray.h"

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

bool Union::intersects(const Ray& ray) {
  for (Surfaces::const_iterator i = surfaces().begin(); i != surfaces().end(); ++i) {
    if ((*i)->intersects(ray)) {
      return true;
    }
  }
  return false;
}