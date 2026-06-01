#include "render/RayCaster.h"
#include "render/State.h"
#include "render/materials/MatteMaterial.h"
#include "render/State.h"
#include "render/RayCaster.h"
#include "core/math/HitPoint.h"
#include "render/brdf/Lambertian.h"
#include "render/primitives/Scene.h"
#include "render/lights/Light.h"
#include "core/math/Ray.h"
#include "render/RayCaster.h"
#include "render/State.h"
#include "render/textures/Texture.h"

#include <algorithm>

using namespace std;
using namespace render;

Colord MatteMaterial::shade(const render::RayCaster*, const render::Scene& scene, const Rayd& ray,
                            const HitPoint& hitPoint, render::State& state) const {
  auto texColor = diffuseTexture() ? diffuseTexture()->evaluate(ray, hitPoint) : Colord::black();

  render::Lambertian ambientBRDF(texColor, ambientCoefficient());
  render::Lambertian diffuseBRDF(texColor, diffuseCoefficient());

  // for diffuse BRDFs the in and out vectors are irrelevant, so let's not calculate them
  auto color = ambientBRDF.reflectance(hitPoint, Vector3d::null) * scene.ambient();

  for (const auto& light : scene.lights()) {
    Vector3d in = light->direction(hitPoint.point());

    if (scene.intersects(Rayd(hitPoint.point(), in).epsilonShifted(), state)) {
      state.shadowHit(this, "MatteMaterial");
    } else {
      state.shadowMiss(this, "MatteMaterial");
      double normalDotIn = hitPoint.normal() * in;
      if (normalDotIn > 0.0)
        color +=
          diffuseBRDF(hitPoint, Vector3d::null, Vector3d::null) * light->radiance() * normalDotIn;
    }
  }

  return color;
}

render::WhittedShadeResult MatteMaterial::shadeWhitted(const render::RayCaster* raycaster,
                                                       const render::Scene& scene, const Rayd& ray,
                                                       const HitPoint& hitPoint,
                                                       render::State& state) const {
  return render::WhittedShadeResult{MatteMaterial::shade(raycaster, scene, ray, hitPoint, state),
                                    {}};
}

Colord MatteMaterial::evalBsdf(const HitPoint& hitPoint, const Vector3d& wi,
                               const Vector3d& wo) const {
  return diffuseLobe(nullptr, hitPoint).eval(hitPoint, wi, wo);
}

render::MaterialBsdfSample MatteMaterial::sampleBsdf(const HitPoint& hitPoint, const Vector3d& wi,
                                                     const Vector2d& sample) const {
  render::MaterialBsdfSample result;
  Lambertian lobe = diffuseLobe(nullptr, hitPoint);
  result.value = lobe.sample(hitPoint, wi, result.direction, result.pdf, sample);
  result.isDelta = false;
  return result;
}

double MatteMaterial::bsdfPdf(const HitPoint& hitPoint, const Vector3d& wi,
                              const Vector3d& wo) const {
  return diffuseLobe(nullptr, hitPoint).pdf(hitPoint, wi, wo);
}

Lambertian MatteMaterial::diffuseLobe(const Rayd* ray, const HitPoint& hitPoint) const {
  Colord texColor = Colord::black();
  if (auto texture = diffuseTexture()) {
    // Texture::evaluate takes a Rayd; when called from the path tracer's BSDF
    // path we don't have the incoming ray. Synthesize a placeholder ray along
    // the hit normal so UV-derived textures still evaluate deterministically.
    Rayd surrogate = ray ? *ray : Rayd(hitPoint.point() - hitPoint.normal(), hitPoint.normal());
    texColor = texture->evaluate(surrogate, hitPoint);
  }
  return Lambertian(texColor, diffuseCoefficient());
}
