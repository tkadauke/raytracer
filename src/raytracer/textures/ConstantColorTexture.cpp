#include "raytracer/textures/ConstantColorTexture.h"
#include "core/math/Ray.h"
#include "core/math/HitPoint.h"

using namespace raytracer;

Colord ConstantColorTexture::evaluate(const Ray&, const HitPoint&) const {
  return m_color;
}
