#include "raytracer/Raytracer.h"
#include "raytracer/State.h"
#include "render/materials/MatteMaterial.h"
#include "raytracer/State.h"
#include "raytracer/Raytracer.h"
#include "core/math/HitPoint.h"
#include "render/primitives/Scene.h"
#include "render/lights/Light.h"
#include "core/math/Ray.h"
#include "raytracer/Raytracer.h"
#include "raytracer/State.h"
#include "render/textures/Texture.h"

#include <algorithm>

using namespace std;
using namespace render;
using namespace raytracer;

Colord MatteMaterial::shade(const raytracer::Raytracer* raytracer, const Rayd& ray, const HitPoint& hitPoint, raytracer::State& state) const {
  auto texColor = diffuseTexture() ? diffuseTexture()->evaluate(ray, hitPoint) : Colord::black();

  render::Lambertian ambientBRDF(texColor, ambientCoefficient());
  render::Lambertian diffuseBRDF(texColor, diffuseCoefficient());

  // for diffuse BRDFs the in and out vectors are irrelevant, so let's not calculate them
  auto color = ambientBRDF.reflectance(hitPoint, Vector3d::null()) * raytracer->scene()->ambient();

  for (const auto& light : raytracer->scene()->lights()) {
    Vector3d in = light->direction(hitPoint.point());

    if (raytracer->scene()->intersects(Rayd(hitPoint.point(), in).epsilonShifted(), state)) {
      state.shadowHit(this, "MatteMaterial");
    } else {
      state.shadowMiss(this, "MatteMaterial");
      double normalDotIn = hitPoint.normal() * in;
      if (normalDotIn > 0.0)
        color += diffuseBRDF(hitPoint, Vector3d::null(), Vector3d::null()) * light->radiance() * normalDotIn;
    }
  }

  return color;
}
