#include "raytracer/State.h"
#include "raytracer/primitives/Disk.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

using namespace raytracer;

Primitive* Disk::intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) {
  double t = (m_center - ray.origin()) * m_normal / (ray.direction() * m_normal);
  
  if (t < 0.0001) {
    state.miss("Disk, behind ray");
    return nullptr;
  }
  
  Vector4d hitPoint = ray.at(t);
  
  if (hitPoint.squaredDistanceTo(m_center) < m_squaredRadius) {
    if (-ray.direction() * m_normal < 0.0) {
      hitPoints.addOut(HitPoint(this, t, hitPoint, m_normal));
    } else {
      hitPoints.addIn(HitPoint(this, t, hitPoint, m_normal));
    }
    state.hit("Disk");
    return this;
  }

  state.miss("Disk, ray miss");
  return nullptr;
}

BoundingBoxd Disk::boundingBox() {
  Vector3d radius(m_radius, m_radius, m_radius);
  return BoundingBoxd(m_center - radius, m_center + radius);
}
