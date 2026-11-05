#include "render/textures/UVColorTexture.h"
#include "core/math/HitPoint.h"

using namespace render;

Colord UVColorTexture::colorAt(const Vector2d& uv) {
  return Colord(uv.x(), uv.y(), 0.0);
}

Colord UVColorTexture::evaluate(const Rayd&, const HitPoint& hitPoint) const {
  return colorAt(hitPoint.uv());
}
