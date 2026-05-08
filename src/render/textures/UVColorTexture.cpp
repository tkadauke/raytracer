#include "render/textures/UVColorTexture.h"
#include "core/math/HitPoint.h"

using namespace render;

Colord UVColorTexture::evaluate(const Rayd&, const HitPoint& hitPoint) const {
  return Colord(hitPoint.uv().x(), hitPoint.uv().y(), 0.0);
}
