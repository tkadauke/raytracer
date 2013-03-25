#include "raytracer/primitives/Difference.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Ray.h"

Primitive* Difference::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  bool firstElement = true;
  
  for (Primitives::const_iterator i = primitives().begin(); i != primitives().end(); ++i) {
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
