#include "raytracer/materials/PortalMaterial.h"
#include "raytracer/Raytracer.h"
#include "core/math/HitPoint.h"

using namespace std;

void PortalMaterial::setMatrix(const Matrix4d& matrix) {
  m_originMatrix = matrix.inverted();
  m_directionMatrix = Matrix3d(m_originMatrix);
}

Colord PortalMaterial::shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth) {
  return raytracer->rayColor(transformedRay(ray.from(hitPoint.point()).epsilonShifted()), recursionDepth + 1) * m_filterColor;
}
