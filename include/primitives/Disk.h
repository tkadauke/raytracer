#ifndef DISK_H
#define DISK_H

#include "primitives/Primitive.h"
#include "math/Vector.h"

class Disk : public Primitive {
public:
  Disk(const Vector3d& center, const Vector3d& normal, double radius)
    : Primitive(), m_center(center), m_normal(normal), m_radius(radius), m_squaredRadius(radius * radius)
  {
  }
  
  virtual Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints);
  virtual BoundingBox boundingBox();

private:
  Vector4d m_center;
  Vector3d m_normal;
  double m_radius, m_squaredRadius;
};

#endif
