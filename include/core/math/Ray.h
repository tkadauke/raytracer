#ifndef RAY_H
#define RAY_H

#include "core/math/Vector.h"

class Ray {
public:
  static const double epsilon = 0.001;

  inline Ray(const Vector3d& origin, const Vector3d& direction)
    : m_origin(origin), m_direction(direction)
  {
  }
  
  inline Ray epsilonShifted() const { return Ray(at(Ray::epsilon), m_direction); }
  inline Ray from(const Vector4d& origin) const { return Ray(origin, m_direction); }
  inline Ray to(const Vector3d& direction) const { return Ray(m_origin, direction); }
  
  inline const Vector4d& origin() const { return m_origin; }
  inline const Vector3d& direction() const { return m_direction; }
  
  inline Vector4d at(double t) const { return m_origin + m_direction * t; }
  
protected:
  Vector4d m_origin;
  Vector3d m_direction;
};

#endif
