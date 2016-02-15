#include "raytracer/materials/PhongMaterial.h"
#include "raytracer/Raytracer.h"
#include "core/math/HitPoint.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/Light.h"
#include "core/math/Ray.h"

#include <algorithm>

using namespace std;
using namespace raytracer;

Colord PhongMaterial::shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int) {
  auto color = raytracer->scene()->ambient() * m_diffuseColor;

  for (const auto& light : raytracer->scene()->lights()) {
    Vector3d lightDirection = (light->position() - hitPoint.point()).normalized();
    
    if (!raytracer->scene()->intersects(Ray(hitPoint.point(), lightDirection).epsilonShifted())) {
      Vector3d h = (lightDirection - ray.direction()).normalized();
      color = color + m_diffuseColor * light->color() * max(0.0, hitPoint.normal() * lightDirection) +
              light->color() * m_highlightColor * pow(h * hitPoint.normal(), m_exponent);
    }
  }
  
  return color;
}
