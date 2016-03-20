#include "raytracer/brdf/GlossySpecular.h"
#include "core/math/Constants.h"
#include "core/math/HitPoint.h"

using namespace raytracer;

Colord GlossySpecular::calculate(const HitPoint& hitPoint, const Vector3d& out, const Vector3d& in) const {
  double normalDotIn = hitPoint.normal() * in;
  Vector3d lobeDirection = (-in + hitPoint.normal() * 2.0 * normalDotIn);
  double lobeDotOut = lobeDirection * out;
  if (lobeDotOut > 0.0)
    return specularColor() * specularCoefficient() * pow(lobeDotOut, exponent());

  return Colord::black();
}
