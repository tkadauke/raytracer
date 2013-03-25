#include "raytracer/primitives/Intersection.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

Primitive* Intersection::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  unsigned int numHits = 0;
  for (Primitives::const_iterator i = primitives().begin(); i != primitives().end(); ++i) {
    HitPointInterval candidate;
    if ((*i)->intersect(ray, candidate)) {
      if (numHits) {
        hitPoints = hitPoints & candidate;
      } else {
        hitPoints = candidate;
      }
      numHits++;
    }
  }
  
  return numHits != primitives().size() || hitPoints.empty() ? 0 : this;
}

bool Intersection::intersects(const Ray& ray) {
  for (Primitives::const_iterator i = primitives().begin(); i != primitives().end(); ++i) {
    if (!(*i)->intersects(ray))
      return false;
  }
  
  return true;
}

BoundingBox Intersection::boundingBox() {
  BoundingBox result;
  int num = 0;
  for (Primitives::const_iterator i = primitives().begin(); i != primitives().end(); ++i, ++num) {
    if (num == 0)
      result = (*i)->boundingBox();
    else
      result &= (*i)->boundingBox();
  }
  return result;
}
