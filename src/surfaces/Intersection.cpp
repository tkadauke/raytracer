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
