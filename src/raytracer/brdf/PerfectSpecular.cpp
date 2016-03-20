#include "raytracer/brdf/PerfectSpecular.h"
#include "core/math/Constants.h"
#include "core/math/HitPoint.h"

using namespace raytracer;

Colord PerfectSpecular::sample(const HitPoint& hitPoint, const Vector3d& out, Vector3d& in) const {
  double normalDotOut = hitPoint.normal() * out;
  in = -out + hitPoint.normal() * 2.0 * normalDotOut;

  double normalDotIn = hitPoint.normal() * out;
  return reflectionColor() * reflectionCoefficient() / normalDotIn;
}
