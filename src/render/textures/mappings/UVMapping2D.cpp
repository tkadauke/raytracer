#include "render/textures/mappings/UVMapping2D.h"
#include "core/math/HitPoint.h"

using namespace render;

void UVMapping2D::map(const HitPoint& hitPoint, double& s, double& t) const {
  s = hitPoint.uv().x() * m_uScale;
  t = hitPoint.uv().y() * m_vScale;
}
