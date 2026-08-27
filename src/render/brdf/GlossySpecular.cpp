#include "render/brdf/GlossySpecular.h"
#include "core/math/Constants.h"
#include "core/math/HitPoint.h"
#include "render/brdf/BRDFSampling.h"

#include <cmath>

using namespace render;

Colord GlossySpecular::calculate(const HitPoint& hitPoint, const Vector3d& out,
                                 const Vector3d& in) const {
  Vector3d lobeDirection = (-in).reflect(hitPoint.normal());
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
  return phongLobePdf(n, wi, wo, exponent());
}
