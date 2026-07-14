#include "render/brdf/Lambertian.h"
#include "core/math/Constants.h"
#include "core/math/HitPoint.h"
#include "render/brdf/BRDFSampling.h"

#include <cmath>

using namespace render;

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
