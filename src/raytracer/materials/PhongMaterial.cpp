#include "materials/PhongMaterial.h"
#include "Raytracer.h"
#include "core/math/HitPoint.h"
#include "raytracer/primitives/Scene.h"
#include "Light.h"
#include "core/math/Ray.h"

#include <algorithm>

using namespace std;

Colord PhongMaterial::shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int) {
  Colord color = raytracer->scene()->ambient() * m_diffuseColor;

  for (Scene::Lights::const_iterator l = raytracer->scene()->lights().begin(); l != raytracer->scene()->lights().end(); ++l) {
    Light* light = *l;
    
    Vector3d lightDirection = (light->position() - hitPoint.point()).normalized();
    
    if (!raytracer->scene()->intersects(Ray(hitPoint.point(), lightDirection).epsilonShifted())) {
      Vector3d h = (lightDirection - ray.direction()).normalized();
      color = color + m_diffuseColor * light->color() * max(0.0, hitPoint.normal() * lightDirection) +
               light->color() * m_highlightColor * pow(h * hitPoint.normal(), 128);
    }
  }
  
  return color;
}
