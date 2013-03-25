#include "primitives/Union.h"
#include "math/HitPointInterval.h"
#include "math/Ray.h"

Primitive* Union::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  bool hit = false;
  
  for (Primitives::const_iterator i = primitives().begin(); i != primitives().end(); ++i) {
    HitPointInterval candidate;
    if ((*i)->intersect(ray, candidate)) {
      hit = true;
      hitPoints = hitPoints | candidate;
    }
  }
  
  return hit ? this : 0;
}

bool Union::intersects(const Ray& ray) {
  for (Primitives::const_iterator i = primitives().begin(); i != primitives().end(); ++i) {
    if ((*i)->intersects(ray)) {
      return true;
    }
  }
  return false;
}
