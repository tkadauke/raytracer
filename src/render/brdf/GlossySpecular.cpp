#include "render/brdf/GlossySpecular.h"
#include "core/math/Constants.h"
#include "core/math/HitPoint.h"

#include <algorithm>
#include <cmath>

using namespace render;

namespace {
  Vector3d tangentFor(const Vector3d& n) {
    const Vector3d helper = std::abs(n.y()) < 0.999 ? Vector3d::up() : Vector3d::right();
    return (helper ^ n).normalized();
  }

  Vector3d phongLobeDirection(const Vector3d& axis, const Vector2d& sample, double exponent) {
    const double u0 = std::clamp(sample.x(), 0.0, 1.0);
    const double u1 = std::clamp(sample.y(), 0.0, 1.0);
    const double cosTheta = std::pow(u0, 1.0 / (exponent + 1.0));
    const double sinTheta = std::sqrt(std::max(0.0, 1.0 - cosTheta * cosTheta));
    const double phi = TAU * u1;

    const Vector3d tangent = tangentFor(axis);
    const Vector3d bitangent = axis ^ tangent;
    return (tangent * (sinTheta * std::cos(phi)) + bitangent * (sinTheta * std::sin(phi)) +
            axis * cosTheta)
      .normalized();
  }
}

Colord GlossySpecular::calculate(const HitPoint& hitPoint, const Vector3d& out,
                                 const Vector3d& in) const {
  double normalDotIn = hitPoint.normal() * in;
  Vector3d lobeDirection = (-in + hitPoint.normal() * 2.0 * normalDotIn);
  double lobeDotOut = lobeDirection * out;
  if (lobeDotOut > 0.0)
    return specularColor() * specularCoefficient() * pow(lobeDotOut, exponent());

  return Colord::black();
}

Colord GlossySpecular::sample(const HitPoint& hitPoint, const Vector3d& wi, Vector3d& wo,
                              double& pdfValue, const Vector2d& samplePoint) const {
  const Vector3d n = hitPoint.normal().normalized();
  if (n * wi < 0.0) {
    pdfValue = 0.0;
    wo = Vector3d::undefined;
    return Colord::black();
  }

  const Vector3d lobeAxis = (-wi).reflect(n).normalized();
  wo = phongLobeDirection(lobeAxis, samplePoint, exponent());
  pdfValue = pdf(hitPoint, wi, wo);
  if (pdfValue == 0.0)
    return Colord::black();

  return eval(hitPoint, wi, wo);
}

double GlossySpecular::pdf(const HitPoint& hitPoint, const Vector3d& wi, const Vector3d& wo) const {
  const Vector3d n = hitPoint.normal().normalized();
  if (n * wi < 0.0 || n * wo < 0.0)
    return 0.0;

  const Vector3d lobeAxis = (-wi).reflect(n).normalized();
  const double lobeDotOut = lobeAxis * wo.normalized();
  if (lobeDotOut <= 0.0)
    return 0.0;

  return ((exponent() + 1.0) * invTAU) * std::pow(lobeDotOut, exponent());
}
