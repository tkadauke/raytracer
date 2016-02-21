#include "raytracer/materials/PhongMaterial.h"
#include "raytracer/Raytracer.h"
#include "core/math/HitPoint.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/Light.h"
#include "core/math/Ray.h"
#include "raytracer/textures/Texture.h"

#include <algorithm>

using namespace std;
using namespace raytracer;

Colord PhongMaterial::shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int) {
  auto texColor = m_texture ? m_texture->evaluate(ray, hitPoint) : Colord::black();

  Lambertian ambientBRDF(texColor, m_ambientCoefficient);
  Lambertian diffuseBRDF(texColor, m_diffuseCoefficient);

  Vector3d out = -ray.direction();
  auto color = ambientBRDF.reflectance(hitPoint, out) * raytracer->scene()->ambient();

  for (const auto& light : raytracer->scene()->lights()) {
    Vector3d in = (light->position() - hitPoint.point()).normalized();
    
    if (!raytracer->scene()->intersects(Ray(hitPoint.point(), in).epsilonShifted())) {
      double normalDotIn = hitPoint.normal() * in;
      if (normalDotIn > 0.0) {
        color += (
          diffuseBRDF(hitPoint, out, in)
        + m_specularBRDF(hitPoint, out, in)
        ) * light->color() * normalDotIn;
      }
    }
  }
  
  return color;
}
