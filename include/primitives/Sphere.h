#ifndef SPHERE_H
#define SPHERE_H

#include "primitives/Primitive.h"
#include "math/Vector.h"

class Sphere : public Primitive {
public:
  Sphere(const Vector3d& origin, double radius)
    : m_origin(origin), m_radius(radius)
  {
  }
  
  virtual Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints);
  virtual bool intersects(const Ray& ray);

  virtual BoundingBox boundingBox();

private:
  Vector3d m_origin;
  double m_radius;
};

#endif
