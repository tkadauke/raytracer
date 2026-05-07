#include "render/brdf/BRDF.h"

using namespace render;

Colord BRDF::calculate(const HitPoint&, const Vector3d&, const Vector3d&) const {
  return Colord::black();
}

Colord BRDF::sample(const HitPoint&, const Vector3d&, Vector3d&) const {
  return Colord::black();
}

Colord BRDF::reflectance(const HitPoint&, const Vector3d&) const {
  return Colord::black();
}
