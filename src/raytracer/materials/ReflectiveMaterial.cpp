#include "raytracer/materials/ReflectiveMaterial.h"
#include "raytracer/State.h"
#include "raytracer/Raytracer.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"

using namespace raytracer;

Colord ReflectiveMaterial::shade(Raytracer* raytracer, const Rayd& ray, const HitPoint& hitPoint, State& state) {
  auto color = PhongMaterial::shade(raytracer, ray, hitPoint, state);

  Vector3d out = - ray.direction();
  Vector3d in;
  Colord refl = m_reflectiveBRDF.sample(hitPoint, out, in);
  Rayd reflected(hitPoint.point(), in);
  
  double normalDotIn = hitPoint.normal() * in;
  
  state.recordEvent("ReflectiveMaterial: Tracing reflection");
  color += refl * raytracer->rayColor(reflected.epsilonShifted(), state) * normalDotIn;

  return color;
}
