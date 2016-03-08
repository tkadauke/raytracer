#include "raytracer/materials/PhongMaterial.h"
#include "raytracer/Raytracer.h"
#include "core/math/HitPoint.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/lights/Light.h"
#include "core/math/Ray.h"
#include "raytracer/textures/Texture.h"

#include <algorithm>

using namespace std;
using namespace raytracer;

Colord PhongMaterial::shade(Raytracer* raytracer, const Rayd& ray, const HitPoint& hitPoint, int) {
  auto texColor = diffuseTexture() ? diffuseTexture()->evaluate(ray, hitPoint) : Colord::black();

  Lambertian ambientBRDF(texColor, ambientCoefficient());
  Lambertian diffuseBRDF(texColor, diffuseCoefficient());

  Vector3d out = -ray.direction();
  auto color = ambientBRDF.reflectance(hitPoint, out) * raytracer->scene()->ambient();

  for (const auto& light : raytracer->scene()->lights()) {
    Vector3d in = light->direction(hitPoint.point());
    
    if (!raytracer->scene()->intersects(Rayd(hitPoint.point(), in).epsilonShifted())) {
      double normalDotIn = hitPoint.normal() * in;
      if (normalDotIn > 0.0) {
        color += (
          diffuseBRDF(hitPoint, out, in)
        + m_specularBRDF(hitPoint, out, in)
        ) * light->radiance() * normalDotIn;
      }
    }
  }
  
  return color;
}
