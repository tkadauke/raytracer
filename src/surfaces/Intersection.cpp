#include "surfaces/Intersection.h"
#include "math/HitPointInterval.h"
#include "math/Ray.h"

Surface* Intersection::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  unsigned int numHits = 0;
  for (Surfaces::const_iterator i = surfaces().begin(); i != surfaces().end(); ++i) {
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
  
  return numHits != surfaces().size() || hitPoints.empty() ? 0 : this;
}

bool Intersection::intersects(const Ray& ray) {
  for (Surfaces::const_iterator i = surfaces().begin(); i != surfaces().end(); ++i) {
    if (!(*i)->intersects(ray))
      return false;
  }
  
  return true;
}

BoundingBox Intersection::boundingBox() {
  BoundingBox result;
  int num = 0;
  for (Surfaces::const_iterator i = surfaces().begin(); i != surfaces().end(); ++i, ++num) {
    if (num == 0)
      result = (*i)->boundingBox();
    else
      result &= (*i)->boundingBox();
  }
  return result;
}
