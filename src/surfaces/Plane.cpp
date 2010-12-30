#include "surfaces/Plane.h"
#include "math/Ray.h"
#include "math/HitPointInterval.h"

Surface* Plane::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  double t = calculateIntersectionDistance(ray);
  
  if (t > 0) {
    hitPoints.add(HitPoint(t, ray.at(t), m_normal));
    return this;
  } else
    return 0;
}

bool Plane::intersects(const Ray& ray) {
  return calculateIntersectionDistance(ray) > 0;
}

double Plane::calculateIntersectionDistance(const Ray& ray) {
  const Vector3d& o = ray.origin(), d = ray.direction();
  
  double angle = m_normal * d;
  if (angle == 0)
    return false;
  
  return -(m_normal * o + m_distance) / angle;
}

BoundingBox Plane::boundingBox() {
  // TODO: figure out what to do here. Throw exception? Return null bounding box? Return infinitely large bounding box?
}
