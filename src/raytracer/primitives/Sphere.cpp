#include "raytracer/primitives/Sphere.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"
#include <cmath>

using namespace std;
using namespace raytracer;

Primitive* Sphere::intersect(const Rayd& ray, HitPointInterval& hitPoints) {
  const Vector3d& o = ray.origin() - m_origin, d = ray.direction();
  
  double od = o * d, dd = d * d;
  double discriminant = od * od - dd * (o * o - m_radius * m_radius);
  
  if (discriminant < 0) {
    return nullptr;
  } else if (discriminant > 0) {
    double discriminantRoot = sqrt(discriminant);
    double t1 = (-od - discriminantRoot) / dd;
    double t2 = (-od + discriminantRoot) / dd;
    if (t1 <= 0 && t2 <= 0)
      return nullptr;
    
    Vector3d hitPoint1 = ray.at(t1),
             hitPoint2 = ray.at(t2);
    
    hitPoints.add(
      HitPoint(this, t1, hitPoint1, (hitPoint1 - m_origin) / m_radius),
      HitPoint(this, t2, hitPoint2, (hitPoint2 - m_origin) / m_radius)
    );
    
    return this;
  }
  return nullptr;
}

bool Sphere::intersects(const Rayd& ray) {
  const Vector3d& o = ray.origin() - m_origin, d = ray.direction();
  
  double od = o * d, dd = d * d;
  double discriminant = od * od - dd * (o * o - m_radius * m_radius);
  
  if (discriminant < 0) {
    return false;
  } else if (discriminant > 0) {
    double discriminantRoot = sqrt(discriminant);
    double t1 = (-od - discriminantRoot) / dd;
    double t2 = (-od + discriminantRoot) / dd;
    if (t1 <= 0 && t2 <= 0)
      return false;
    return true;
  }
  return false;
}

BoundingBoxd Sphere::boundingBox() {
  Vector3d radius(m_radius, m_radius, m_radius);
  return BoundingBoxd(m_origin - radius, m_origin + radius);
}
