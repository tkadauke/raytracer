#include "raytracer/materials/TransparentMaterial.h"
#include "raytracer/State.h"
#include "raytracer/Raytracer.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"

#include <algorithm>

using namespace std;
using namespace raytracer;

Colord TransparentMaterial::shade(Raytracer* raytracer, const Rayd& ray, const HitPoint& hitPoint, State& state) {
  Vector3d out = -ray.direction();
  Vector3d in;
  Colord reflectedColor = m_reflectiveBRDF.sample(hitPoint, out, in);
  Rayd reflected(hitPoint.point(), in);
  
  if (m_specularBTDF.totalInternalReflection(ray, hitPoint)) {
    return raytracer->rayColor(reflected.epsilonShifted(), state);
  } else {
    auto color = PhongMaterial::shade(raytracer, ray, hitPoint, state);
    
    Vector3d trans;
    Colord transmittedColor = m_specularBTDF.sample(hitPoint, out, trans);
    Rayd transmitted(hitPoint.point(), trans);
    
    state.recordEvent("TransparentMaterial: Tracing reflection");
    color += reflectedColor * raytracer->rayColor(reflected.epsilonShifted(), state) * fabs(hitPoint.normal() * in);
    
    state.recordEvent("TransparentMaterial: Tracing transmission");
    color += transmittedColor * raytracer->rayColor(transmitted.epsilonShifted(), state) * fabs(hitPoint.normal() * trans);
    return color;
  }
}
