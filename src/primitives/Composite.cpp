#include "primitives/Composite.h"
#include "math/HitPointInterval.h"
#include "math/Ray.h"

#include <limits>

using namespace std;

Composite::~Composite() {
  for (Primitives::const_iterator i = m_primitives.begin(); i != m_primitives.end(); ++i)
    delete *i;
}

BoundingBox Composite::boundingBox() {
  BoundingBox b;
  for (Primitives::const_iterator i = m_primitives.begin(); i != m_primitives.end(); ++i)
    b.include((*i)->boundingBox());
  return b;
}

Primitive* Composite::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  Primitive* hit = 0;
  double minDistance = numeric_limits<double>::infinity();
  
  for (Primitives::const_iterator i = primitives().begin(); i != primitives().end(); ++i) {
    HitPointInterval candidate;
    Primitive* primitive = (*i)->intersect(ray, candidate);
    if (primitive) {
      double distance = candidate.min().distance();
      if (distance < minDistance) {
        hit = primitive;
        minDistance = distance;
        hitPoints = candidate;
      }
    }
  }
  
  return hit;
}

bool Composite::intersects(const Ray& ray) {
  for (Primitives::const_iterator i = primitives().begin(); i != primitives().end(); ++i) {
    if ((*i)->intersects(ray))
      return true;
  }
  
  return false;
}
