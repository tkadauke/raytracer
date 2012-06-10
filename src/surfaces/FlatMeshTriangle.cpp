#include "surfaces/FlatMeshTriangle.h"
#include "surfaces/Mesh.h"
#include "math/Ray.h"
#include "math/HitPointInterval.h"

Surface* FlatMeshTriangle::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  Vector3d v0(m_mesh->vertices[m_index0].point);
  Vector3d v1(m_mesh->vertices[m_index1].point);
  Vector3d v2(m_mesh->vertices[m_index2].point);
  
  double a = v0.x() - v1.x(), b = v0.x() - v2.x(), c = ray.direction().x(), d = v0.x() - ray.origin().x();
  double e = v0.y() - v1.y(), f = v0.y() - v2.y(), g = ray.direction().y(), h = v0.y() - ray.origin().y();
  double i = v0.z() - v1.z(), j = v0.z() - v2.z(), k = ray.direction().z(), l = v0.z() - ray.origin().z();
  
  double m = f * k - g * j, n = h * k - g * l, p = f * l - h * j;
  double q = g * i - e * k, r = e * l - h * i, s = e * j - f * i;
  
  double invDenom = 1.0 / (a * m + b * q + c * s);
  
  double e1 = d * m - b * n - c * p;
  double beta = e1 * invDenom;
  
  if (beta < 0.0 || beta > 1.0)
    return 0;
  
  double e2 = a * n + d * q + c * r;
  double gamma = e2 * invDenom;
  
  if (gamma < 0.0 || gamma > 1.0)
    return 0;
  
  if (beta + gamma > 1.0)
    return 0;
  
  double e3 = a * p - b * r + d * s;
  double t = e3 * invDenom;
  
  if (t < 0.0001)
    return 0;
  
  Vector3d hitPoint = ray.at(t);
  hitPoints.add(HitPoint(t, hitPoint, m_normal));
  return this;
}

Vector3d FlatMeshTriangle::computeNormal() const {
  Vector3d n0(m_mesh->vertices[m_index0].normal);
  Vector3d n1(m_mesh->vertices[m_index1].normal);
  Vector3d n2(m_mesh->vertices[m_index2].normal);
  
  return (n0 + n1 + n2).normalized();
}