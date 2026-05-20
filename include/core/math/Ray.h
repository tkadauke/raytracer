#pragma once

#include "core/math/Vector.h"

#include <iostream>

/**
  * Represents a Ray \f$r = o + td\f$ in three-dimensional space. The Ray has an
  * origin \f$r\f$ and a direction \f$d\f$. This class is used to calculate
  * intersections with various types of objects.
  * 
  * @htmlonly
  * <script type="text/javascript" src="figure.js"></script>
  * <script type="text/javascript" src="ray_class.js"></script>
  * @endhtmlonly
  *
  * @tparam T the vector coordinate type. Usually a floating point type like
  *   float or double.
  */
template<class T>
class Ray {
public:
  /**
    * Minimum distance a Ray has to travel to not be considered at the origin.
    * Specializations for float and double live below the class definition.
    */
  static const T epsilon;

  /**
    * @returns an undefined Ray.
    */
  [[nodiscard]] inline static const Ray& undefined() {
    static Ray r(Vector4<T>::undefined(), Vector3<T>::undefined());
    return r;
  }

  /**
    * Constructs a Ray \f$r = o + td\f$, where origin is \f$o\f$ and direction
    * is \f$d\f$.
    */
  inline explicit Ray(const Vector4<T>& origin, const Vector3<T>& direction)
    : m_origin(origin),
      m_direction(direction)
  {
  }

  /**
    * @returns a Ray with the same direction as this one, but the origin shifted
    *   by epsilon along the direction: \f$o' = o + \epsilon d\f$.
    */
  [[nodiscard]] inline Ray<T> epsilonShifted() const noexcept {
    return Ray<T>(at(Ray::epsilon), m_direction);
  }

  /**
    * @returns a Ray with the same direction as this ray, but with the given
    *   origin.
    */
  [[nodiscard]] inline Ray<T> from(const Vector4<T>& origin) const noexcept {
    return Ray<T>(origin, m_direction);
  }

  /**
    * @returns a Ray with the same origin as this ray, but with the given
    *   direction.
    */
  [[nodiscard]] inline Ray<T> to(const Vector3<T>& direction) const noexcept {
    return Ray<T>(m_origin, direction);
  }

  /**
    * @returns this Ray's origin.
    */
  [[nodiscard]] inline const Vector4<T>& origin() const noexcept {
    return m_origin;
  }

  /**
    * @returns this Ray's direction.
    */
  [[nodiscard]] inline const Vector3<T>& direction() const noexcept {
    return m_direction;
  }

  /**
    * @returns the solution to the Ray's equation \f$r = o + td\f$ for the given
    *   t, i.e. the point along the Ray with distance t from the origin.
    *
    * The following interactive figure illustrates the geometry. Use the `t`
    * slider to move the resulting point along the ray, shown in red.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="ray_at.js"></script>
    * @endhtmlonly
    */
  [[nodiscard]] inline Vector4<T> at(T t) const noexcept {
    return Vector3<T>(m_origin) + m_direction * t;
  }

  /**
    * @returns the distance of the orthogonal projection point of point onto
    *   this ray.
    *
    * @see project().
    */
  [[nodiscard]] inline T projectedDistance(const Vector3<T>& point) const {
    return (direction() * (point - origin())) / (direction() * direction());
  }

  /**
    * @returns the orthogonal projection of point onto this ray.
    *
    * The following figure shows a few random points that are being projected
    * onto a ray.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="ray_project.js"></script>
    * @endhtmlonly
    */
  [[nodiscard]] inline Vector3<T> project(const Vector3<T>& point) const {
    return at(projectedDistance(point));
  }

  /**
    * @returns the shortest distance between point and this ray.
    *
    * @see project().
    */
  [[nodiscard]] inline T distanceTo(const Vector3<T>& point) const {
    return (point - project(point)).length();
  }
  
private:
  Vector4<T> m_origin;
  Vector3<T> m_direction;
};

template<> inline const float  Ray<float>::epsilon  = 0.0001f;
template<> inline const double Ray<double>::epsilon = 0.0000001;

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
