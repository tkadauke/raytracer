#ifndef SPHERE_H
#define SPHERE_H

#include "surfaces/Surface.h"
#include "math/Vector.h"

class Sphere : public Surface {
public:
  Sphere(const Vector3d& origin, double radius)
    : m_origin(origin), m_radius(radius)
  {
  }
  
  virtual Surface* intersect(const Ray& ray, HitPointInterval& hitPoints);
  virtual bool intersects(const Ray& ray);

  virtual BoundingBox boundingBox();

private:
  Vector3d m_origin;
  double m_radius;
};

#endif
