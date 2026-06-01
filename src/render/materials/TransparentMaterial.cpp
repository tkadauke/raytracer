#include "render/RayCaster.h"
#include "render/State.h"
#include "render/materials/TransparentMaterial.h"
#include "render/State.h"
#include "render/RayCaster.h"
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
