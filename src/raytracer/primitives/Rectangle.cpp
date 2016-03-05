#include "raytracer/primitives/Rectangle.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

using namespace raytracer;

Primitive* Rectangle::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  double t = (m_corner - ray.origin()) * m_normal / (ray.direction() * m_normal);
  if (t < 0.0001 || std::isinf(t))
    return nullptr;
  
  Vector3d hitPoint = ray.at(t);
  Vector3d difference = hitPoint - m_corner;
  
  double dot1 = difference * m_leg1;
  
  if (dot1 < 0 || dot1 > m_squaredLength1)
    return nullptr;
  
  double dot2 = difference * m_leg2;
  
  if (dot2 < 0 || dot2 > m_squaredLength2)
    return nullptr;
  
  hitPoints.add(HitPoint(t, hitPoint, m_normal));
  return this;
}

BoundingBoxd Rectangle::boundingBox() {
  BoundingBoxd b;
  b.include(m_corner);
  b.include(m_corner + m_leg1);
  b.include(m_corner + m_leg2);
  b.include(m_corner + m_leg1 + m_leg2);
  return b;
}
