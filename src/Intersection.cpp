#include "Intersection.h"
#include "HitPointInterval.h"
#include "Ray.h"

#include <limits>

using namespace std;

Surface* Intersection::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  double minDistance = numeric_limits<double>::infinity();
  
  int numHits = 0;
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
