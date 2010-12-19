#include "surfaces/Instance.h"
#include "math/Ray.h"
#include "math/HitPointInterval.h"

Surface* Instance::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  Surface* result = m_surface->intersect(instancedRay(ray), hitPoints);
  if (result) {
    hitPoints = hitPoints.transform(m_pointMatrix, m_normalMatrix);
    return this;
  }
  return 0;
}

bool Instance::intersects(const Ray& ray) {
  return m_surface->intersects(instancedRay(ray));
}

void Instance::setMatrix(const Matrix4d& matrix) {
  m_pointMatrix = matrix;
  m_originMatrix = matrix.inverted();
  m_directionMatrix = Matrix3d(m_originMatrix);
  m_normalMatrix = m_directionMatrix.transposed();
}
