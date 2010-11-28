#include "Instance.h"
#include "Ray.h"
#include "HitPointInterval.h"

Surface* Instance::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  Ray instancedRay = Ray(m_originMatrix * ray.origin(), m_directionMatrix * ray.direction());
  Surface* result = m_surface->intersect(instancedRay, hitPoints);
  if (result) {
    hitPoints = hitPoints.transform(m_pointMatrix, m_normalMatrix);
    return this;
  }
  return 0;
}

void Instance::setMatrix(const Matrix4d& matrix) {
  m_pointMatrix = matrix;
  m_originMatrix = matrix.inverted();
  m_directionMatrix = Matrix3d(m_originMatrix);
  m_normalMatrix = m_directionMatrix.transposed();
}
