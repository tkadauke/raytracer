#ifndef PLANE_H
#define PLANE_H

#include "Surface.h"
#include "Vector.h"

class Plane : public Surface {
public:
  Plane(const Vector3d& normal, double distance)
    : m_normal(normal), m_distance(distance) {}
  
  virtual Surface* intersect(const Ray& ray, HitPointInterval& hitPoints);

private:
  Vector3d m_normal;
  double m_distance;
};

#endif
