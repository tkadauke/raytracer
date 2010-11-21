#ifndef RAY_H
#define RAY_H

#include "Vector.h"

class Ray {
public:
  static const double epsilon = 0.001;

  inline Ray(const Vector3d& origin, const Vector3d& direction)
    : m_origin(origin), m_direction(direction)
  {
  }
  
  inline Ray epsilonShifted() const { return Ray(at(Ray::epsilon), m_direction); }
  
  inline const Vector3d& origin() const { return m_origin; }
  inline const Vector3d& direction() const { return m_direction; }
  
  inline Vector3d at(double t) const { return m_origin + m_direction * t; }
  
protected:
  Vector3d m_origin, m_direction;
};

#endif
