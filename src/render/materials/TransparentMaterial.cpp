#include "render/RayCaster.h"
#include "render/State.h"
#include "render/brdf/BRDFSampling.h"
#include "render/materials/TransparentMaterial.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"

#include <algorithm>

using namespace std;
using namespace render;

Colord TransparentMaterial::shade(const render::RayCaster* raycaster, const render::Scene& scene,
                                  const Rayd& ray, const HitPoint& hitPoint,
                                  render::State& state) const {
  Vector3d out = -ray.direction();
  Vector3d in;
  Colord reflectedColor = m_reflectiveBRDF.sample(hitPoint, out, in);
  Rayd reflected(hitPoint.point(), in);

  auto color = PhongMaterial::shade(raycaster, scene, ray, hitPoint, state);

  if (m_specularBTDF.totalInternalReflection(ray, hitPoint)) {
    // TIR: full energy reflected, throughput unchanged (coefficient = 1.0).
    state.recordEvent(this, "TransparentMaterial: TIR, tracing full mirror reflection");
    color += raycaster->rayColor(reflected.epsilonShifted(), state);
  } else {
    Vector3d trans;
    Colord transmittedColor = m_specularBTDF.sample(hitPoint, out, trans);
    Rayd transmitted(hitPoint.point(), trans);

    state.withThroughput(
      state.throughput * reflectedColor.max() * fabs(hitPoint.normal() * in), [&] {
        state.recordEvent(this, "TransparentMaterial: Tracing reflection");
        color += reflectedColor * raycaster->rayColor(reflected.epsilonShifted(), state) *
                 fabs(hitPoint.normal() * in);
      });

    state.withThroughput(
      state.throughput * transmittedColor.max() * fabs(hitPoint.normal() * trans), [&] {
        state.recordEvent(this, "TransparentMaterial: Tracing transmission");
        color += transmittedColor * raycaster->rayColor(transmitted.epsilonShifted(), state) *
                 fabs(hitPoint.normal() * trans);
      });
  }

  return color;
}

render::WhittedShadeResult TransparentMaterial::shadeWhitted(const render::RayCaster* raycaster,
                                                             const render::Scene& scene,
                                                             const Rayd& ray,
                                                             const HitPoint& hitPoint,
                                                             render::State& state) const {
  Vector3d out = -ray.direction();
  Vector3d in;
  Colord reflectedColor = m_reflectiveBRDF.sample(hitPoint, out, in);
  Rayd reflected(hitPoint.point(), in);

  render::WhittedShadeResult result =
    PhongMaterial::shadeWhitted(raycaster, scene, ray, hitPoint, state);

  if (m_specularBTDF.totalInternalReflection(ray, hitPoint)) {
    result.continuations.push_back(
      render::WhittedContinuation{reflected.epsilonShifted(), Colord::white(), 1.0});
    return result;
  }

  Vector3d trans;
  Colord transmittedColor = m_specularBTDF.sample(hitPoint, out, trans);
  Rayd transmitted(hitPoint.point(), trans);

  const double reflectionScale = fabs(hitPoint.normal() * in);
  result.continuations.push_back(
    render::WhittedContinuation{reflected.epsilonShifted(), reflectedColor * reflectionScale,
                                reflectedColor.max() * reflectionScale});

  const double transmissionScale = fabs(hitPoint.normal() * trans);
  result.continuations.push_back(
    render::WhittedContinuation{transmitted.epsilonShifted(), transmittedColor * transmissionScale,
                                transmittedColor.max() * transmissionScale});

  return result;
}

Colord TransparentMaterial::evalBsdf(const HitPoint& hitPoint, const Vector3d& wi,
                                     const Vector3d& wo) const {
  return PhongMaterial::evalBsdf(hitPoint, wi, wo);
}

render::MaterialBsdfSample TransparentMaterial::sampleBsdf(const HitPoint& hitPoint,
                                                           const Vector3d& wi,
                                                           const Vector2d& sample) const {
  const Rayd incidentRay(hitPoint.point(), -wi);
  const bool isTotalInternalReflection =
    m_specularBTDF.totalInternalReflection(incidentRay, hitPoint);
  if (isTotalInternalReflection) {
    return sampleTotalInternalReflectionBsdf(hitPoint, wi);
  }

  const BsdfSamplingWeights weights = bsdfSamplingWeights(false);
  const double selector = clampUnit(sample.x());

  if (weights.reflection > 0.0 && selector < weights.reflection) {
    return sampleReflectionBsdf(hitPoint, wi, weights.reflection);
  }

  if (weights.transmission > 0.0) {
    return sampleTransmissionBsdf(hitPoint, wi, weights.transmission);
  }

  return MaterialBsdfSample();
}

std::vector<render::MaterialBsdfSample>
TransparentMaterial::deltaBsdfSamples(const HitPoint& hitPoint, const Vector3d& wi) const {
  const Rayd incidentRay(hitPoint.point(), -wi);
  if (m_specularBTDF.totalInternalReflection(incidentRay, hitPoint)) {
    return {sampleTotalInternalReflectionBsdf(hitPoint, wi)};
  }

  std::vector<MaterialBsdfSample> samples;
  if (reflectionCoefficient() > 0.0 && reflectionColor() != Colord::black()) {
    samples.push_back(sampleReflectionBsdf(hitPoint, wi, /*selectionWeight=*/1.0));
  }
  if (transmissionCoefficient() > 0.0) {
    samples.push_back(sampleTransmissionBsdf(hitPoint, wi, /*selectionWeight=*/1.0));
  }
  return samples;
}

double TransparentMaterial::bsdfPdf(const HitPoint& hitPoint, const Vector3d& wi,
                                    const Vector3d& wo) const {
  (void)hitPoint;
  (void)wi;
  (void)wo;
  return 0.0;
}

TransparentMaterial::BsdfSamplingWeights
TransparentMaterial::bsdfSamplingWeights(bool totalInternalReflection) const {
  if (totalInternalReflection) {
    return BsdfSamplingWeights{0.0, 1.0, 0.0};
  }

  const double reflection = std::max(0.0, reflectionCoefficient());
  const double transmission = std::max(0.0, transmissionCoefficient());
  const double total = reflection + transmission;
  if (total <= 0.0) {
    return BsdfSamplingWeights{0.0, 0.0, 0.0};
  }
  return BsdfSamplingWeights{0.0, reflection / total, transmission / total};
}

render::MaterialBsdfSample TransparentMaterial::sampleReflectionBsdf(const HitPoint& hitPoint,
                                                                     const Vector3d& wi,
                                                                     double selectionWeight) const {
  render::MaterialBsdfSample result;
  Vector3d direction;
  const Colord reflectedColor = m_reflectiveBRDF.sample(hitPoint, wi, direction);
  const double reflectionScale = fabs(hitPoint.normal() * direction);
  result.direction = direction.normalized();
  result.value = reflectedColor * (reflectionScale / selectionWeight);
  result.pdf = 1.0;
  result.isDelta = true;
  return result;
}

render::MaterialBsdfSample
TransparentMaterial::sampleTransmissionBsdf(const HitPoint& hitPoint, const Vector3d& wi,
                                            double selectionWeight) const {
  render::MaterialBsdfSample result;
  Vector3d direction;
  const Colord transmittedColor = m_specularBTDF.sample(hitPoint, wi, direction);
  const double transmissionScale = fabs(hitPoint.normal() * direction);
  result.direction = direction.normalized();
  result.value = transmittedColor * (transmissionScale / selectionWeight);
  result.pdf = 1.0;
  result.isDelta = true;
  return result;
}

render::MaterialBsdfSample
TransparentMaterial::sampleTotalInternalReflectionBsdf(const HitPoint& hitPoint,
                                                       const Vector3d& wi) const {
  render::MaterialBsdfSample result;
  result.direction = (-wi).reflect(hitPoint.normal()).normalized();
  result.value = Colord::white();
  result.pdf = 1.0;
  result.isDelta = true;
  return result;
}
