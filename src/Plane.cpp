#include "Plane.h"
#include "Ray.h"
#include "HitPointInterval.h"

Surface* Plane::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  const Vector3d& o = ray.origin(), d = ray.direction();
  
  double angle = m_normal * d;
  if (angle == 0)
    return false;
  
  double t = -(m_normal * o + m_distance) / angle;
  
  if (t > 0) {
    hitPoints.add(HitPoint(t, ray.at(t), m_normal));
    return this;
  } else
    return 0;
}
