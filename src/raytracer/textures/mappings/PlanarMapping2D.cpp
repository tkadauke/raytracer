#include "raytracer/textures/mappings/PlanarMapping2D.h"
#include "core/math/HitPoint.h"

using namespace raytracer;

void PlanarMapping2D::map(const HitPoint& hitPoint, double& s, double& t) const {
  s = hitPoint.point() * Vector3d::right();
  t = hitPoint.point() * Vector3d::forward();
}
