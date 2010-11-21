#ifndef SPHERE_H
#define SPHERE_H

#include "Surface.h"
#include "Vector.h"

class Sphere : public Surface {
public:
  Sphere(const Vector3d& origin, double radius)
    : m_origin(origin), m_radius(radius)
  {
  }
  
  virtual Surface* intersect(const Ray& ray, HitPointInterval& hitPoints);

private:
  Vector3d m_origin;
  double m_radius;
};

#endif
