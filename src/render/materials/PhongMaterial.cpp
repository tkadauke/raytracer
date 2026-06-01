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

Colord PhongMaterial::shade(const render::RayCaster*, const render::Scene& scene, const Rayd& ray,
                            const HitPoint& hitPoint, render::State& state) const {
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
        color += (diffuseBRDF(hitPoint, out, in) + m_specularBRDF(hitPoint, out, in)) *
                 light->radiance() * normalDotIn;
      }
    }
  }

  return color;
}

render::WhittedShadeResult PhongMaterial::shadeWhitted(const render::RayCaster* raycaster,
                                                       const render::Scene& scene, const Rayd& ray,
                                                       const HitPoint& hitPoint,
                                                       render::State& state) const {
  return render::WhittedShadeResult{PhongMaterial::shade(raycaster, scene, ray, hitPoint, state),
                                    {}};
}

Colord PhongMaterial::evalBsdf(const HitPoint& hitPoint, const Vector3d& wi,
                               const Vector3d& wo) const {
  return MatteMaterial::evalBsdf(hitPoint, wi, wo) + m_specularBRDF.eval(hitPoint, wi, wo);
}

render::MaterialBsdfSample PhongMaterial::sampleBsdf(const HitPoint& hitPoint, const Vector3d& wi,
                                                     const Vector2d& sample) const {
  render::MaterialBsdfSample result;
  const double diffuseWeight = diffuseSamplingWeight();
  const double specularWeight = 1.0 - diffuseWeight;
  const double selector = std::clamp(sample.x(), 0.0, 1.0);
  const double y = std::clamp(sample.y(), 0.0, 1.0);

  if (specularWeight == 0.0 || selector < diffuseWeight) {
    const double remappedX = diffuseWeight > 0.0 ? selector / diffuseWeight : selector;
    Lambertian lobe = diffuseLobe(nullptr, hitPoint);
    lobe.sample(hitPoint, wi, result.direction, result.pdf, Vector2d(remappedX, y));
  } else {
    const double remappedX =
      specularWeight > 0.0 ? (selector - diffuseWeight) / specularWeight : selector;
    m_specularBRDF.sample(hitPoint, wi, result.direction, result.pdf, Vector2d(remappedX, y));
  }

  result.value = evalBsdf(hitPoint, wi, result.direction);
  result.pdf = bsdfPdf(hitPoint, wi, result.direction);
  result.isDelta = false;
  return result;
}

double PhongMaterial::bsdfPdf(const HitPoint& hitPoint, const Vector3d& wi,
                              const Vector3d& wo) const {
  const double diffuseWeight = diffuseSamplingWeight();
  const double specularWeight = 1.0 - diffuseWeight;
  return diffuseWeight * MatteMaterial::bsdfPdf(hitPoint, wi, wo) +
         specularWeight * m_specularBRDF.pdf(hitPoint, wi, wo);
}

double PhongMaterial::diffuseSamplingWeight() const {
  const double diffuse = std::max(0.0, diffuseCoefficient());
  const double specular = std::max(0.0, specularCoefficient());
  const double total = diffuse + specular;
  return total <= 0.0 ? 1.0 : diffuse / total;
}
