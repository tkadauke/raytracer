#ifndef RECTANGLE_H
#define RECTANGLE_H

#include "surfaces/Surface.h"
#include "math/Vector.h"

class Rectangle : public Surface {
public:
  Rectangle(const Vector3d& corner, const Vector3d& leg1, const Vector3d& leg2)
    : Surface(), m_corner(corner), m_leg1(leg1), m_leg2(leg2)
  {
    m_normal = (m_leg1 ^ m_leg2).normalized();
    m_squaredLength1 = m_leg1.squaredLength();
    m_squaredLength2 = m_leg2.squaredLength();
  }
  
  virtual Surface* intersect(const Ray& ray, HitPointInterval& hitPoints);
  virtual BoundingBox boundingBox();

private:
  Vector4d m_corner;
  Vector3d m_leg1, m_leg2, m_normal;
  double m_squaredLength1, m_squaredLength2;
};

#endif
