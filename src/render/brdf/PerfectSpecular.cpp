#include "render/brdf/PerfectSpecular.h"
#include "core/math/Constants.h"
#include "core/math/HitPoint.h"

using namespace render;

Colord PerfectSpecular::sample(const HitPoint& hitPoint, const Vector3d& out, Vector3d& in) const {
  in = (-out).reflect(hitPoint.normal());
  double normalDotIn = hitPoint.normal() * out;
  return reflectionColor() * reflectionCoefficient() / normalDotIn;
}
