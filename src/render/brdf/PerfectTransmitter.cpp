#include "render/brdf/PerfectTransmitter.h"
#include "core/math/Ray.h"
#include "core/math/HitPoint.h"

using namespace render;

Colord PerfectTransmitter::sample(const HitPoint& hitPoint, const Vector3d& out,
                                  Vector3d& in) const {
  Vector3d n = hitPoint.normal();
  double cosTheta = n * out;
  double eta = refractionIndex();

  if (cosTheta < 0.0) {
    n = -n;
    eta = 1.0 / eta;
  }

  in = out.refract(n, eta);

  return Colord::white() * (transmissionCoefficient() / (eta * eta) / fabs(hitPoint.normal() * in));
}

bool PerfectTransmitter::totalInternalReflection(const Rayd& ray, const HitPoint& hitPoint) const {
  Vector3d wo = -ray.direction();
  double cosTheta = hitPoint.normal() * wo;
  double eta = refractionIndex();

  if (cosTheta < 0)
    eta = 1.0 / eta;

  return (1.0 - (1.0 - cosTheta * cosTheta) / (eta * eta) < 0.0);
}
