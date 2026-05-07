#include "render/bsdf/BSDF.h"

using namespace render;

Colord BSDF::reflectance(const HitPoint&, const Vector3d&) const {
  return Colord::black();
}
