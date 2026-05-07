#include "raytracer/Raytracer.h"
#include "render/State.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/State.h"
#include "raytracer/Raytracer.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"

using namespace render;
using namespace raytracer;

Colord ReflectiveMaterial::shade(const raytracer::Raytracer* raytracer, const Rayd& ray, const HitPoint& hitPoint, render::State& state) const {
  auto color = PhongMaterial::shade(raytracer, ray, hitPoint, state);

  Vector3d out = - ray.direction();
  Vector3d in;
  Colord refl = m_reflectiveBRDF.sample(hitPoint, out, in);
  Rayd reflected(hitPoint.point(), in);

  double normalDotIn = hitPoint.normal() * in;

  state.recordEvent(this, "ReflectiveMaterial: Tracing reflection");
  color += refl * raytracer->rayColor(reflected.epsilonShifted(), state) * normalDotIn;

  return color;
}
