#include "raytracer/primitives/Triangle.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

using namespace raytracer;

Primitive* Triangle::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  double a = m_point0.x() - m_point1.x(), b = m_point0.x() - m_point2.x(), c = ray.direction().x(), d = m_point0.x() - ray.origin().x();
  double e = m_point0.y() - m_point1.y(), f = m_point0.y() - m_point2.y(), g = ray.direction().y(), h = m_point0.y() - ray.origin().y();
  double i = m_point0.z() - m_point1.z(), j = m_point0.z() - m_point2.z(), k = ray.direction().z(), l = m_point0.z() - ray.origin().z();
  
  double m = f * k - g * j, n = h * k - g * l, p = f * l - h * j;
  double q = g * i - e * k, r = e * l - h * i, s = e * j - f * i;
  
  double invDenom = 1.0 / (a * m + b * q + c * s);
  
  double e1 = d * m - b * n - c * p;
  double beta = e1 * invDenom;
  
  if (beta < 0.0 || beta > 1.0)
    return nullptr;
  
  double e2 = a * n + d * q + c * r;
  double gamma = e2 * invDenom;
  
  if (gamma < 0.0 || gamma > 1.0)
    return nullptr;
  
  if (beta + gamma > 1.0)
    return nullptr;
  
  double e3 = a * p - b * r + d * s;
  double t = e3 * invDenom;
  
  if (t < 0.0001)
    return nullptr;
  
  Vector3d hitPoint = ray.at(t);
  hitPoints.add(HitPoint(this, t, hitPoint, m_normal));
  return this;
}

Vector3d Triangle::computeNormal() const {
  Vector3d normal = (m_point1 - m_point0) ^ (m_point2 - m_point0);
  return normal.normalized();
}

BoundingBoxd Triangle::boundingBox() {
  BoundingBoxd b;
  b.include(m_point0);
  b.include(m_point1);
  b.include(m_point2);
  return b;
}
