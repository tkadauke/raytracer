#include "raytracer/materials/PortalMaterial.h"
#include "raytracer/Raytracer.h"
#include "core/math/HitPoint.h"

using namespace std;
using namespace raytracer;

void PortalMaterial::setMatrix(const Matrix4d& matrix) {
  m_originMatrix = matrix.inverted();
  m_directionMatrix = Matrix3d(m_originMatrix);
}

Colord PortalMaterial::shade(const Raytracer* raytracer, const Rayd& ray, const HitPoint& hitPoint, State& state) const {
  return raytracer->rayColor(transformedRay(ray.from(hitPoint.point()).epsilonShifted()), state) * m_filterColor;
}
