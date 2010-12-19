#include "materials/MatteMaterial.h"
#include "Raytracer.h"
#include "math/HitPoint.h"
#include "surfaces/Scene.h"
#include "Light.h"
#include "math/Ray.h"

#include <algorithm>

using namespace std;

Colord MatteMaterial::shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int) {
  Colord color = raytracer->scene()->ambient() * m_diffuseColor;

  for (Scene::Lights::const_iterator l = raytracer->scene()->lights().begin(); l != raytracer->scene()->lights().end(); ++l) {
    Light* light = *l;
    
    Vector3d lightDirection = (light->position() - hitPoint.point()).normalized();
    
    if (!raytracer->scene()->intersects(Ray(hitPoint.point(), lightDirection).epsilonShifted())) {
      Vector3d h = (lightDirection - ray.direction()).normalized();
      color = color + m_diffuseColor * light->color() * max(0.0, hitPoint.normal() * lightDirection);
    }
  }
  
  return color;
}
