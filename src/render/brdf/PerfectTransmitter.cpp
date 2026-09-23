#include "render/brdf/PerfectTransmitter.h"
#include "render/brdf/BRDFSampling.h"
#include "core/math/Ray.h"
#include "core/math/HitPoint.h"

using namespace render;

Colord PerfectTransmitter::sample(const HitPoint& hitPoint, const Vector3d& out,
                                  Vector3d& in) const {
  const Vector3d n = hitPoint.normal();
  const double eta = orientedRefractionEta(n * out, refractionIndex());

  in = refractedDirection(n, out, refractionIndex());

  return Colord::white() * (transmissionCoefficient() / (eta * eta) / fabs(hitPoint.normal() * in));
}

bool PerfectTransmitter::totalInternalReflection(const Rayd& ray, const HitPoint& hitPoint) const {
  const Vector3d wo = -ray.direction();
  const double cosTheta = hitPoint.normal() * wo;
  const double eta = orientedRefractionEta(cosTheta, refractionIndex());

  return totalInternalReflects(cosTheta, eta);
}
