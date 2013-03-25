#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "raytracer/primitives/Primitive.h"
#include "core/math/Vector.h"

class Triangle : public Primitive {
public:
  Triangle(const Vector3d& a, const Vector3d& b, const Vector3d& c)
    : Primitive(), m_point0(a), m_point1(b), m_point2(c)
  {
    m_normal = computeNormal();
  }
  
  virtual BoundingBox boundingBox();
  virtual Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints);

private:
  Vector3d computeNormal() const;
  
  Vector3d m_point0, m_point1, m_point2;
  Vector3d m_normal;
};

#endif
