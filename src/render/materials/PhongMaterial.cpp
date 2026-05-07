#include "render/RayCaster.h"
#include "render/State.h"
#include "render/materials/PhongMaterial.h"
#include "render/State.h"
#include "render/RayCaster.h"
#include "core/math/HitPoint.h"
#include "render/primitives/Scene.h"
#include "render/lights/Light.h"
#include "core/math/Ray.h"
#include "render/RayCaster.h"
#include "render/State.h"
#include "render/textures/Texture.h"

#include <algorithm>

using namespace std;
using namespace render;

Colord PhongMaterial::shade(const render::RayCaster* raycaster, const render::Scene& scene, const Rayd& ray, const HitPoint& hitPoint, render::State& state) const {
  auto texColor = diffuseTexture() ? diffuseTexture()->evaluate(ray, hitPoint) : Colord::black();

  render::Lambertian ambientBRDF(texColor, ambientCoefficient());
  render::Lambertian diffuseBRDF(texColor, diffuseCoefficient());

  Vector3d out = -ray.direction();
  auto color = ambientBRDF.reflectance(hitPoint, out) * scene.ambient();

  for (const auto& light : scene.lights()) {
    Vector3d in = light->direction(hitPoint.point());

    if (scene.intersects(Rayd(hitPoint.point(), in).epsilonShifted(), state)) {
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
