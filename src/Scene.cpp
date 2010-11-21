#include "Scene.h"
#include "HitPointInterval.h"
#include "Ray.h"
#include "Light.h"

#include <limits>

using namespace std;

Scene::~Scene() {
  for (Lights::const_iterator i = m_lights.begin(); i != m_lights.end(); ++i) {
    delete *i;
  }
}

Surface* Scene::intersect(const Ray& ray, HitPointInterval& hitPoints) {
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
