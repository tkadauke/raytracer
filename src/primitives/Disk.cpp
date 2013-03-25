#include "primitives/Disk.h"
#include "math/Ray.h"
#include "math/HitPointInterval.h"

Primitive* Disk::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  double t = (m_center - ray.origin()) * m_normal / (ray.direction() * m_normal);
  
  if (t < 0.0001)
    return 0;
  
  Vector3d hitPoint = ray.at(t);
  
  if (hitPoint.squaredDistanceTo(m_center) < m_squaredRadius) {
    hitPoints.add(HitPoint(t, hitPoint, m_normal));
    return this;
  }
  return 0;
}

BoundingBox Disk::boundingBox() {
  Vector3d radius(m_radius, m_radius, m_radius);
  return BoundingBox(m_center - radius, m_center + radius);
}
