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

Colord TransparentMaterial::shade(const render::RayCaster* raycaster, const render::Scene& scene, const Rayd& ray, const HitPoint& hitPoint, render::State& state) const {
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

    double savedThroughput = state.throughput;

    state.throughput = savedThroughput * reflectedColor.max() * fabs(hitPoint.normal() * in);
    state.recordEvent(this, "TransparentMaterial: Tracing reflection");
    color += reflectedColor * raycaster->rayColor(reflected.epsilonShifted(), state) * fabs(hitPoint.normal() * in);

    state.throughput = savedThroughput * transmittedColor.max() * fabs(hitPoint.normal() * trans);
    state.recordEvent(this, "TransparentMaterial: Tracing transmission");
    color += transmittedColor * raycaster->rayColor(transmitted.epsilonShifted(), state) * fabs(hitPoint.normal() * trans);

    state.throughput = savedThroughput;
  }

  return color;
}
