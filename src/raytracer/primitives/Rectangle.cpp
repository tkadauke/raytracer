#include "raytracer/State.h"
#include "raytracer/primitives/Rectangle.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

using namespace raytracer;

Primitive* Rectangle::intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) {
  double t = (m_corner - ray.origin()) * m_normal / (ray.direction() * m_normal);
  if (t < 0.0001 || std::isinf(t)) {
    state.miss("Rectangle, behind ray or parallel");
    return nullptr;
  }
  
  Vector3d hitPoint = ray.at(t);
  Vector3d difference = hitPoint - m_corner;
  
  double dot1 = difference * m_leg1;
  
  if (dot1 < 0 || dot1 > m_squaredLength1) {
    state.miss("Rectangle, outside u axis");
    return nullptr;
  }
  
  double dot2 = difference * m_leg2;
  
  if (dot2 < 0 || dot2 > m_squaredLength2) {
    state.miss("Rectangle, outside v axis");
    return nullptr;
  }
  
  hitPoints.add(HitPoint(this, t, hitPoint, m_normal));
  state.hit("Rectangle");
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
