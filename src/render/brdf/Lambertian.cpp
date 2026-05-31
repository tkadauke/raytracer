#include "render/brdf/Lambertian.h"
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

  Vector3d cosineHemisphereDirection(const Vector3d& n, const Vector2d& sample) {
    const double u0 = std::clamp(sample.x(), 0.0, 1.0);
    const double u1 = std::clamp(sample.y(), 0.0, 1.0);
    const double r = std::sqrt(u0);
    const double phi = TAU * u1;
    const double x = r * std::cos(phi);
    const double y = r * std::sin(phi);
    const double z = std::sqrt(std::max(0.0, 1.0 - u0));

    const Vector3d tangent = tangentFor(n);
    const Vector3d bitangent = n ^ tangent;
    return (tangent * x + bitangent * y + n * z).normalized();
  }
}

Colord Lambertian::calculate(const HitPoint&, const Vector3d&, const Vector3d&) const {
  return diffuseColor() * reflectionCoefficient() * invPI;
}

Colord Lambertian::reflectance(const HitPoint&, const Vector3d&) const {
  return diffuseColor() * reflectionCoefficient();
}

Colord Lambertian::sample(const HitPoint& hitPoint, const Vector3d& wi, Vector3d& wo, double& pdfValue,
                          const Vector2d& samplePoint) const {
  wo = cosineHemisphereDirection(hitPoint.normal().normalized(), samplePoint);
  pdfValue = pdf(hitPoint, wi, wo);
  if (pdfValue == 0.0)
    return Colord::black();

  return eval(hitPoint, wi, wo);
}

double Lambertian::pdf(const HitPoint& hitPoint, const Vector3d& wi, const Vector3d& wo) const {
  const Vector3d n = hitPoint.normal().normalized();
  if (n * wi < 0.0)
    return 0.0;

  const double normalDotOut = n * wo;
  if (normalDotOut <= 0.0)
    return 0.0;

  return normalDotOut * invPI;
}
