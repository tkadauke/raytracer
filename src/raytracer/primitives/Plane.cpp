#include "raytracer/primitives/Plane.h"
#include "core/math/Ray.h"
#include "core/math/HitPointInterval.h"

using namespace raytracer;

Primitive* Plane::intersect(const Ray& ray, HitPointInterval& hitPoints) {
  double t = calculateIntersectionDistance(ray);
  
  if (t > 0) {
    hitPoints.add(HitPoint(this, t, ray.at(t), m_normal));
    return this;
  } else
    return nullptr;
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

BoundingBoxd Plane::boundingBox() {
  return BoundingBoxd::undefined();
}
