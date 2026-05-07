#include "render/RayCaster.h"
#include "render/State.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/State.h"
#include "render/RayCaster.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"

using namespace render;

Colord ReflectiveMaterial::shade(const render::RayCaster* raycaster, const render::Scene& scene, const Rayd& ray, const HitPoint& hitPoint, render::State& state) const {
  auto color = PhongMaterial::shade(raycaster, scene, ray, hitPoint, state);

  Vector3d out = - ray.direction();
  Vector3d in;
  Colord refl = m_reflectiveBRDF.sample(hitPoint, out, in);
  Rayd reflected(hitPoint.point(), in);

  double normalDotIn = hitPoint.normal() * in;

  state.recordEvent(this, "ReflectiveMaterial: Tracing reflection");
  color += refl * raycaster->rayColor(reflected.epsilonShifted(), state) * normalDotIn;

  return color;
}
