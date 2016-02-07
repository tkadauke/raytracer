#include "raytracer/primitives/Instance.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

#include <vector>

using namespace std;
using namespace raytracer;

Primitive* Instance::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  Primitive* result = m_primitive->intersect(instancedRay(ray), hitPoints);
  if (result) {
    hitPoints = hitPoints.transform(m_pointMatrix, m_normalMatrix);
    return this;
  }
  return nullptr;
}

bool Instance::intersects(const Ray& ray) {
  return m_primitive->intersects(instancedRay(ray));
}

void Instance::setMatrix(const Matrix4d& matrix) {
  m_pointMatrix = matrix;
  m_originMatrix = matrix.inverted();
  m_directionMatrix = Matrix3d(m_originMatrix);
  m_normalMatrix = m_directionMatrix.transposed();
}

Material* Instance::material() const {
  Material* parent = Primitive::material();
  if (parent)
    return parent;
  else
    return m_primitive->material();
}

BoundingBox Instance::boundingBox() {
  BoundingBox original = m_primitive->boundingBox();
  vector<Vector3d> vertices;
  original.getVertices(vertices);
  
  BoundingBox result;
  for (vector<Vector3d>::const_iterator i = vertices.begin(); i != vertices.end(); ++i) {
    result.include(m_pointMatrix * Vector4d(*i));
  }
  return result;
}
