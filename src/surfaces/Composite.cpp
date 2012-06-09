#include "surfaces/Composite.h"
#include "math/HitPointInterval.h"
#include "math/Ray.h"

#include <limits>

using namespace std;

Composite::~Composite() {
  for (Surfaces::const_iterator i = m_surfaces.begin(); i != m_surfaces.end(); ++i)
    delete *i;
}

BoundingBox Composite::boundingBox() {
  BoundingBox b;
  for (Surfaces::const_iterator i = m_surfaces.begin(); i != m_surfaces.end(); ++i)
    b.include((*i)->boundingBox());
  return b;
}

Surface* Composite::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  Surface* hit = 0;
  double minDistance = numeric_limits<double>::infinity();
  
  for (Surfaces::const_iterator i = surfaces().begin(); i != surfaces().end(); ++i) {
    HitPointInterval candidate;
    Surface* surface = (*i)->intersect(ray, candidate);
    if (surface) {
      double distance = candidate.min().distance();
      if (distance < minDistance) {
        hit = surface;
        minDistance = distance;
        hitPoints = candidate;
      }
    }
  }
  
  return hit;
}

bool Composite::intersects(const Ray& ray) {
  for (Surfaces::const_iterator i = surfaces().begin(); i != surfaces().end(); ++i) {
    if ((*i)->intersects(ray))
      return true;
  }
  
  return false;
}
