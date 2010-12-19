#ifndef PLANE_H
#define PLANE_H

#include "surfaces/Surface.h"
#include "math/Vector.h"

class Plane : public Surface {
public:
  Plane(const Vector3d& normal, double distance)
    : m_normal(normal), m_distance(distance) {}
  
  virtual Surface* intersect(const Ray& ray, HitPointInterval& hitPoints);
  virtual bool intersects(const Ray& ray);

private:
  double calculateIntersectionDistance(const Ray& ray);
  
  Vector3d m_normal;
  double m_distance;
};

#endif
