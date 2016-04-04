#pragma once

#include "core/math/Vector.h"
#include "core/math/Matrix.h"
#include "core/InequalityOperator.h"
#include <cmath>
#include <iostream>

namespace raytracer {
  class Primitive;
}

/**
  * This class keeps track of ray/object intersection information. It holds the
  * ray distance (the distance between the ray origin and the intersection
  * point), the intersection point and the normal vector at the point of
  * intersection. The following figure shows how everything fits together. A ray
  * originating at \f$r\f$ hit a sphere at the hit point \f$p\f$ with distance
  * \f$d\f$ and normal \f$n\f$ at the hit point.
  * 
  * @htmlonly
  * <script type="text/javascript" src="figure.js"></script>
  * <script type="text/javascript" src="hitpoint_class.js"></script>
  * @endhtmlonly
  * 
  * Note that this class does not calculate \f$p\f$, \f$n\f$, or \f$d\f$; it
  * merely stores the information, which has to be calculated elsewhere. It does
  * however provide a few methods to perform calculations on this data:
  * swappedNormal() turns the normal in the opposite direction, which is
  * important for when a ray travels on the inside of objects; transformed()
  * performs a linear transform on the hitpoint and normal, which is interesting
  * during instancing (see raytracer::Instance).
  * 
  * This class inherits from InequalityOperator, which provides operator !=.
  */
class HitPoint : public InequalityOperator<HitPoint> {
public:
  /**
    * Holds the "undefined" HitPoint.
    */
  static const HitPoint& undefined();
  
  /**
    * Default constructor. Constructs the null HitPoint.
    */
  inline HitPoint()
    : m_primitive(nullptr),
      m_distance(0)
  {
  }
  
  /**
    * Constructs a HitPoint on @p primitive from the specified @p distance,
    * @p point, and @p normal.
    */
  inline explicit HitPoint(const raytracer::Primitive* primitive, double distance, const Vector4d& point, const Vector3d& normal)
    : m_primitive(primitive),
      m_distance(distance),
      m_point(point),
      m_normal(normal)
  {
  }
  
  /**
    * @returns the primitive that contains this HitPoint.
    */
  inline const raytracer::Primitive* primitive() const {
    return m_primitive;
  }
  
  /**
    * Sets the @p primitive for this HitPoint.
    */
  inline void setPrimitive(const raytracer::Primitive* primitive) {
    m_primitive = primitive;
  }

  /**
    * @returns the ray distance of this HitPoint.
    */
  inline double distance() const {
    return m_distance;
  }
  
  /**
    * Sets the ray @p distance for this HitPoint.
    */
  inline void setDistance(double distance) {
    m_distance = distance;
  }
  
  /**
    * @returns true if this HitPoint's distance is smaller than @p other's
    *   distance, false otherwise.
    */
  inline bool operator<(const HitPoint& other) const {
    return distance() < other.distance();
  }
  
  /**
    * @returns the HitPoint's intersection point.
    */
  inline const Vector4d& point() const {
    return m_point;
  }
  
  /**
    * Sets the HitPoint's intersection @p point.
    */
  inline void setPoint(const Vector4d& point) {
    m_point = point;
  }
  
  /**
    * @returns the normal vector at the intersection point.
    */
  inline const Vector3d& normal() const {
    return m_normal;
  }
  
  /**
    * Sets HitPoint's @p normal vector.
    */
  inline void setNormal(const Vector3d& normal) {
    m_normal = normal;
  }
  
  /**
    * @returns a copy of this HitPoint with the normal swapped.
    */
  inline HitPoint swappedNormal() const {
    return HitPoint(m_primitive, m_distance, m_point, -m_normal);
  }
  
  /**
    * @returns a copy of this HitPoint, with the intersection point transformed
    *   with @p pointMatrix, and the normal transformed with @p normalMatrix.
    *   The resulting normal is then normalized.
    */
  inline HitPoint transform(const Matrix4d& pointMatrix, const Matrix3d& normalMatrix) const {
    return HitPoint(m_primitive, m_distance, pointMatrix * m_point, (normalMatrix * m_normal).normalized());
  }
  
  /**
    * @returns true if this HitPoint's distance, point and normal are equal to
    *   @p other's, false otherwise.
    */
  inline bool operator==(const HitPoint& other) const {
    if (&other == this)
      return true;
    return m_primitive == other.primitive() &&
           m_distance == other.distance() &&
           m_point == other.point() &&
           m_normal == other.normal();
  }
  
  /**
    * @returns true if this HitPoint's distance is NaN, or either the point or
    *   normal is undefined.
    */
  inline bool isUndefined() const {
    return std::isnan(m_distance) || m_point.isUndefined() || m_normal.isUndefined();
  }
  
private:
  const raytracer::Primitive* m_primitive;
  double m_distance;
  Vector4d m_point;
  Vector3d m_normal;
};

/**
  * Outputs the given HitPoint to the given std::ostream @p os.
  * 
  * @returns @p os.
  */
std::ostream& operator<<(std::ostream& os, const HitPoint& point);
