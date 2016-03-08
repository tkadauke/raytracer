#pragma once

#include "core/math/Vector.h"

#include <iostream>

/**
  * Represents a Ray in three-dimensional space. The Ray has an origin and a
  * direction. This class is used to calculate intersections with various types
  * of objects.
  * 
  * @tparam T the vector coordinate type. Usually a floating point type like
  *   float or double.
  */
template<class T>
class Ray {
public:
  /**
    * Minimum distance a Ray has to travel to not be considered at the origin.
    */
  static const T epsilon;

  /**
    * @returns an undefined Ray.
    */
  inline static const Ray& undefined() {
    static Ray r(Vector4<T>::undefined(), Vector3<T>::undefined());
    return r;
  }

  /**
    * Constructs a Ray \f$r = o + td\f$, where origin is \f$o\f$ and direction
    * is \f$d\f$.
    */
  inline Ray(const Vector4<T>& origin, const Vector3<T>& direction)
    : m_origin(origin),
      m_direction(direction)
  {
  }
  
  /**
    * @returns a Ray with the same direction as this one, but the origin shifted
    *   by epsilon along the direction: \f$o' = o + \epsilon d\f$.
    */
  inline Ray<T> epsilonShifted() const {
    return Ray<T>(at(Ray::epsilon), m_direction);
  }
  
  /**
    * @returns a Ray with the same direction as this ray, but with the given
    *   origin.
    */
  inline Ray<T> from(const Vector4<T>& origin) const {
    return Ray<T>(origin, m_direction);
  }
  
  /**
    * @returns a Ray with the same origin as this ray, but with the given
    *   direction.
    */
  inline Ray<T> to(const Vector3<T>& direction) const {
    return Ray<T>(m_origin, direction);
  }
  
  /**
    * @returns this Ray's origin.
    */
  inline const Vector4<T>& origin() const {
    return m_origin;
  }
  
  /**
    * @returns this Ray's direction.
    */
  inline const Vector3<T>& direction() const {
    return m_direction;
  }
  
  /**
    * @returns the solution to the Ray's equation \f$r = o + td\f$ for the given
    *   t, i.e. the point along the Ray with distance t from the origin.
    */
  inline Vector4<T> at(T t) const {
    return Vector3<T>(m_origin) + m_direction * t;
  }
  
private:
  Vector4<T> m_origin;
  Vector3<T> m_direction;
};

/**
  * Serializes ray to the given std::ostream.
  * 
  * @returns os.
  */
template<class T>
inline std::ostream& operator<<(std::ostream& os, const Ray<T>& ray) {
  os << ray.origin() << "->" << ray.direction();
  return os;
}

/**
  * Shortcut for a Ray using float vectors.
  */
typedef Ray<float> Rayf;

/**
  * Shortcut for a Ray using double vectors.
  */
typedef Ray<double> Rayd;
