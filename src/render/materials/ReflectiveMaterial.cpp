#include "render/RayCaster.h"
#include "render/State.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/State.h"
#include "render/RayCaster.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"

#include <algorithm>

using namespace render;

Colord ReflectiveMaterial::shade(const render::RayCaster* raycaster, const render::Scene& scene,
                                 const Rayd& ray, const HitPoint& hitPoint,
                                 render::State& state) const {
  auto color = PhongMaterial::shade(raycaster, scene, ray, hitPoint, state);

  Vector3d out = -ray.direction();
  Vector3d in;
  Colord refl = m_reflectiveBRDF.sample(hitPoint, out, in);
  Rayd reflected(hitPoint.point(), in);

  double normalDotIn = hitPoint.normal() * in;

  state.withThroughput(state.throughput * refl.max() * normalDotIn, [&] {
    state.recordEvent(this, "ReflectiveMaterial: Tracing reflection");
    color += refl * raycaster->rayColor(reflected.epsilonShifted(), state) * normalDotIn;
  });

  return color;
}

render::WhittedShadeResult ReflectiveMaterial::shadeWhitted(const render::RayCaster* raycaster,
                                                            const render::Scene& scene,
                                                            const Rayd& ray,
                                                            const HitPoint& hitPoint,
                                                            render::State& state) const {
  render::WhittedShadeResult result =
    PhongMaterial::shadeWhitted(raycaster, scene, ray, hitPoint, state);

  Vector3d out = -ray.direction();
  Vector3d in;
  Colord refl = m_reflectiveBRDF.sample(hitPoint, out, in);
  double normalDotIn = hitPoint.normal() * in;

  result.continuations.push_back(render::WhittedContinuation{
    Rayd(hitPoint.point(), in).epsilonShifted(), refl * normalDotIn, refl.max() * normalDotIn});
  return result;
}

Colord ReflectiveMaterial::evalBsdf(const HitPoint& hitPoint, const Vector3d& wi,
                                    const Vector3d& wo) const {
  return PhongMaterial::evalBsdf(hitPoint, wi, wo);
}

render::MaterialBsdfSample ReflectiveMaterial::sampleBsdf(const HitPoint& hitPoint,
                                                          const Vector3d& wi,
                                                          const Vector2d& sample) const {
  render::MaterialBsdfSample result;
  const double reflectionWeight = reflectionSamplingWeight();
  const double localWeight = 1.0 - reflectionWeight;
  const double selector = std::clamp(sample.x(), 0.0, 1.0);
  const double y = std::clamp(sample.y(), 0.0, 1.0);

  if (reflectionWeight > 0.0 && (localWeight == 0.0 || selector >= localWeight)) {
    result.direction = (-wi).reflect(hitPoint.normal()).normalized();
    result.value = reflectionColor() * (reflectionCoefficient() / reflectionWeight);
    result.pdf = 1.0;
    result.isDelta = true;
    return result;
  }

  const double remappedX = localWeight > 0.0 ? selector / localWeight : selector;
  result = PhongMaterial::sampleBsdf(hitPoint, wi, Vector2d(remappedX, y));
  result.pdf = bsdfPdf(hitPoint, wi, result.direction);
  return result;
}

double ReflectiveMaterial::bsdfPdf(const HitPoint& hitPoint, const Vector3d& wi,
                                   const Vector3d& wo) const {
  const double localWeight = 1.0 - reflectionSamplingWeight();
  return localWeight * PhongMaterial::bsdfPdf(hitPoint, wi, wo);
}

double ReflectiveMaterial::reflectionSamplingWeight() const {
  const double local = std::max(0.0, diffuseCoefficient()) + std::max(0.0, specularCoefficient());
  const double reflection = std::max(0.0, reflectionCoefficient());
  const double total = local + reflection;
  return total <= 0.0 ? 0.0 : reflection / total;
}
