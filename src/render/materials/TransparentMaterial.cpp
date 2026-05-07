#include "raytracer/Raytracer.h"
#include "raytracer/State.h"
#include "render/materials/TransparentMaterial.h"
#include "raytracer/State.h"
#include "raytracer/Raytracer.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"

#include <algorithm>

using namespace std;
using namespace render;
using namespace raytracer;

Colord TransparentMaterial::shade(const raytracer::Raytracer* raytracer, const Rayd& ray, const HitPoint& hitPoint, raytracer::State& state) const {
  Vector3d out = -ray.direction();
  Vector3d in;
  Colord reflectedColor = m_reflectiveBRDF.sample(hitPoint, out, in);
  Rayd reflected(hitPoint.point(), in);

  auto color = PhongMaterial::shade(raytracer, ray, hitPoint, state);

  if (m_specularBTDF.totalInternalReflection(ray, hitPoint)) {
    state.recordEvent(this, "TransparentMaterial: TIR, tracing full mirror reflection");
    color += raytracer->rayColor(reflected.epsilonShifted(), state);
  } else {
    Vector3d trans;
    Colord transmittedColor = m_specularBTDF.sample(hitPoint, out, trans);
    Rayd transmitted(hitPoint.point(), trans);

    state.recordEvent(this, "TransparentMaterial: Tracing reflection");
    color += reflectedColor * raytracer->rayColor(reflected.epsilonShifted(), state) * fabs(hitPoint.normal() * in);

    state.recordEvent(this, "TransparentMaterial: Tracing transmission");
    color += transmittedColor * raytracer->rayColor(transmitted.epsilonShifted(), state) * fabs(hitPoint.normal() * trans);
  }

  return color;
}
