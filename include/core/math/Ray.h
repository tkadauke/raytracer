#pragma once

#include "core/math/Vector.h"

#include <iostream>

/**
  * Represents a Ray in three-dimensional space. The Ray has an origin and a
  * direction. This class is used to calculate intersections with various types
  * of objects.
  */
class Ray {
public:
  /**
    * Minimum distance a Ray has to travel to not be considered at the origin.
    */
  static const double epsilon;

  /**
    * @returns an undefined Ray.
    */
  inline static const Ray& undefined() {
    static Ray r(Vector4d::undefined(), Vector3d::undefined());
    return r;
  }

  /**
    * Constructs a Ray \f$r = o + td\f$, where origin is \f$o\f$ and direction
    * is \f$d\f$.
    */
  inline Ray(const Vector4d& origin, const Vector3d& direction)
    : m_origin(origin),
      m_direction(direction)
  {
  }
  
  /**
    * @returns a Ray with the same direction as this one, but the origin shifted
    *   by epsilon along the direction: \f$o' = o + \epsilon d\f$.
    */
  inline Ray epsilonShifted() const {
    return Ray(at(Ray::epsilon), m_direction);
  }
  
  /**
    * @returns a Ray with the same direction as this ray, but with the given
    *   origin.
    */
  inline Ray from(const Vector4d& origin) const {
    return Ray(origin, m_direction);
  }
  
  /**
    * @returns a Ray with the same origin as this ray, but with the given
    *   direction.
    */
  inline Ray to(const Vector3d& direction) const {
    return Ray(m_origin, direction);
  }
  
  /**
    * @returns this Ray's origin.
    */
  inline const Vector4d& origin() const {
    return m_origin;
  }
  
  /**
    * @returns this Ray's direction.
    */
  inline const Vector3d& direction() const {
    return m_direction;
  }
  
  /**
    * @returns the solution to the Ray's equation \f$r = o + td\f$ for the given
    *   t, i.e. the point along the Ray with distance t from the origin.
    */
  inline Vector4d at(double t) const {
    return Vector3d(m_origin) + m_direction * t;
  }
  
protected:
  Vector4d m_origin;
  Vector3d m_direction;
};

/**
  * Serializes ray to the given std::ostream.
  * 
  * @returns os.
  */
inline std::ostream& operator<<(std::ostream& os, const Ray& ray) {
  os << ray.origin() << "->" << ray.direction();
  return os;
}
