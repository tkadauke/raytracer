#include "raytracer/textures/CheckerBoardTexture.h"
#include "raytracer/textures/mappings/TextureMapping2D.h"
#include "core/math/Ray.h"
#include "core/math/HitPoint.h"

using namespace raytracer;

Colord CheckerBoardTexture::evaluate(const Rayd& ray, const HitPoint& hitPoint) const {
  double s, t;
  m_mapping->map(hitPoint, s, t);
  if ((int(floor(s)) + int(floor(t))) % 2 == 0) {
    return m_brightTexture->evaluate(ray, hitPoint);
  } else {
    return m_darkTexture->evaluate(ray, hitPoint);
  }
}
