#include "render/brdf/BRDF.h"

using namespace render;

Colord BRDF::calculate(const HitPoint&, const Vector3d&, const Vector3d&) const {
  return Colord::black();
}

Colord BRDF::sample(const HitPoint&, const Vector3d&, Vector3d&) const {
  return Colord::black();
}

double BRDF::pdf(const HitPoint&, const Vector3d&, const Vector3d&) const {
  return 0.0;
}

Colord BRDF::sample(const HitPoint& hitPoint, const Vector3d& wi, Vector3d& wo, double& pdf,
                    const Vector2d&) const {
  pdf = 0.0;
  return sample(hitPoint, wi, wo);
}
