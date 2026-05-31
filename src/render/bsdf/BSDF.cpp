#include "render/bsdf/BSDF.h"

using namespace render;

Colord BSDF::reflectance(const HitPoint&, const Vector3d&) const {
  return Colord::black();
}

Colord BSDF::sample(const HitPoint& hitPoint, const Vector3d& wi, Vector3d& wo, double& pdf,
                    const Vector2d&) const {
  return sample(hitPoint, wi, wo, pdf);
}
