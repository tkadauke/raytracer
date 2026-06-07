#include "render/RayCaster.h"
#include "render/State.h"
#include "render/materials/ReflectiveMaterial.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"

#include <cmath>

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
                                                          const Vector2d&) const {
  render::MaterialBsdfSample result;
  if (reflectionCoefficient() <= 0.0 || reflectionColor() == Colord::black()) {
    return result;
  }

  Vector3d direction;
  const Colord reflected = m_reflectiveBRDF.sample(hitPoint, wi, direction);
  const double reflectionScale = std::fabs(hitPoint.normal() * direction);
  result.direction = direction.normalized();
  result.value = reflected * reflectionScale;
  result.pdf = 1.0;
  result.isDelta = true;
  return result;
}

double ReflectiveMaterial::bsdfPdf(const HitPoint&, const Vector3d&, const Vector3d&) const {
  return 0.0;
}
