#include "render/brdf/PerfectTransmitter.h"
#include "core/math/Ray.h"
#include "core/math/HitPoint.h"

using namespace render;

Colord PerfectTransmitter::sample(const HitPoint& hitPoint, const Vector3d& out,
                                  Vector3d& in) const {
  const auto refraction = out.orientedRefraction(hitPoint.normal(), refractionIndex());
  in = out.refract(refraction.normal, refraction.eta);

  return Colord::white() * (transmissionCoefficient() / (refraction.eta * refraction.eta) /
                            fabs(hitPoint.normal() * in));
}

bool PerfectTransmitter::totalInternalReflection(const Rayd& ray, const HitPoint& hitPoint) const {
  Vector3d wo = -ray.direction();
  return wo.orientedRefraction(hitPoint.normal(), refractionIndex()).totalInternalReflection;
}
