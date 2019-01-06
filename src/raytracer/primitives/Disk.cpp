#include "raytracer/State.h"
#include "raytracer/primitives/Disk.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"
#include "core/math/Number.h"

using namespace raytracer;

const Primitive* Disk::intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const {
  double t = (m_center - ray.origin()) * m_normal / (ray.direction() * m_normal);

  Vector4d hitPoint = ray.at(t);

  if (hitPoint.squaredDistanceTo(m_center) < m_squaredRadius) {
    if (-ray.direction() * m_normal < 0.0) {
      hitPoints.addOut(HitPoint(this, t, hitPoint, m_normal));
    } else {
      hitPoints.addIn(HitPoint(this, t, hitPoint, m_normal));
    }

    if (t < 0.0001) {
      state.miss(this, "Disk behind ray");
      return nullptr;
    } else {
      state.hit(this, "Disk");
      return this;
    }
  }

  state.miss(this, "Disk, ray miss");
  return nullptr;
}

BoundingBoxd Disk::calculateBoundingBox() const {
  Vector3d radius(m_radius, m_radius, m_radius);
  return BoundingBoxd(m_center - radius, m_center + radius);
}

Vector3d Disk::farthestPoint(const Vector3d& direction) const {
  Vector3d directionOnPlane = direction - m_normal * (direction * m_normal);
  if (isAlmostZero(directionOnPlane.length())) {
    return m_center;
  } else {
    return m_center + directionOnPlane.normalized() * m_radius;
  }
}
