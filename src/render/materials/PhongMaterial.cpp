#include "raytracer/Raytracer.h"
#include "raytracer/State.h"
#include "render/materials/PhongMaterial.h"
#include "raytracer/State.h"
#include "raytracer/Raytracer.h"
#include "core/math/HitPoint.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/lights/Light.h"
#include "core/math/Ray.h"
#include "raytracer/Raytracer.h"
#include "raytracer/State.h"
#include "render/textures/Texture.h"

#include <algorithm>

using namespace std;
using namespace render;
using namespace raytracer;

Colord PhongMaterial::shade(const raytracer::Raytracer* raytracer, const Rayd& ray, const HitPoint& hitPoint, raytracer::State& state) const {
  auto texColor = diffuseTexture() ? diffuseTexture()->evaluate(ray, hitPoint) : Colord::black();

  render::Lambertian ambientBRDF(texColor, ambientCoefficient());
  render::Lambertian diffuseBRDF(texColor, diffuseCoefficient());

  Vector3d out = -ray.direction();
  auto color = ambientBRDF.reflectance(hitPoint, out) * raytracer->scene()->ambient();

  for (const auto& light : raytracer->scene()->lights()) {
    Vector3d in = light->direction(hitPoint.point());

    if (raytracer->scene()->intersects(Rayd(hitPoint.point(), in).epsilonShifted(), state)) {
      state.shadowHit(this, "PhongMaterial");
    } else {
      state.shadowMiss(this, "PhongMaterial");
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
