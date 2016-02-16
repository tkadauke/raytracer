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
  Vector3d out = -ray.direction();
  auto color = m_ambientBRDF.reflectance(hitPoint, out) * raytracer->scene()->ambient();

  for (const auto& light : raytracer->scene()->lights()) {
    Vector3d lightDirection = (light->position() - hitPoint.point()).normalized();
    
    if (!raytracer->scene()->intersects(Ray(hitPoint.point(), lightDirection).epsilonShifted())) {
      double normalDotIn = hitPoint.normal() * lightDirection;
      if (normalDotIn > 0.0) {
        color += (
          m_diffuseBRDF(hitPoint, lightDirection, out)
        + m_specularBRDF(hitPoint, lightDirection, out)
        ) * light->color() * normalDotIn;
      }
    }
  }
  
  return color;
}
