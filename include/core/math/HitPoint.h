#pragma once

#include "core/math/Vector.h"
#include "core/math/Matrix.h"
#include "core/InequalityOperator.h"
#include <cmath>
#include <iostream>

/**
  * This class keeps track of ray/object intersection information. It holds the
  * ray distance (the distance between the ray origin and the intersection
  * point), the intersection point and the normal vector at the point of
  * intersection.
  * 
  * Inherits from InequalityOperator, which provides operator !=.
  */
class HitPoint : public InequalityOperator<HitPoint> {
public:
  /**
    * Holds the "undefined" HitPoint.
    */
  static const HitPoint& undefined();
  
  /**
    * Constructs the null HitPoint.
    */
  inline HitPoint()
    : m_distance(0)
  {
  }
  
  /**
    * Constructs a HitPoint from the specified distance, point, and normal.
    */
  inline HitPoint(double distance, const Vector4d& point, const Vector3d& normal)
    : m_distance(distance),
      m_point(point),
      m_normal(normal)
  {
  }
  
  /**
    * @returns the ray distance of this HitPoint.
    */
  inline double distance() const {
    return m_distance;
  }
  
  /**
    * Sets the ray distance for this HitPoint.
    */
  inline void setDistance(double distance) {
    m_distance = distance;
  }
  
  /**
    * @returns true if this HitPoint's distance is smaller than other's
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
    * Sets the HitPoint's intersection point.
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
    * Sets HitPoint's normal vector.
    */
  inline void setNormal(const Vector3d& normal) {
    m_normal = normal;
  }
  
  /**
    * @returns a copy of this HitPoint with the normal swapped.
    */
  inline HitPoint swappedNormal() const {
    return HitPoint(m_distance, m_point, -m_normal);
  }
  
  /**
    * @returns a copy of this HitPoint, with the intersection point transformed
    *   with pointMatrix, and the normal transformed with normalMatrix.
    */
  inline HitPoint transform(const Matrix4d& pointMatrix, const Matrix3d& normalMatrix) const {
    return HitPoint(m_distance, pointMatrix * m_point, (normalMatrix * m_normal).normalized());
  }
  
  /**
    * @returns true if this HitPoint's distance, point and normal are equal to
    *   other's, false otherwise.
    */
  inline bool operator==(const HitPoint& other) const {
    if (&other == this)
      return true;
    return m_distance == other.distance() &&
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
  double m_distance;
  Vector4d m_point;
  Vector3d m_normal;
};

/**
  * Outputs the given HitPoint to the given std::ostream.
  * 
  * @returns os.
  */
std::ostream& operator<<(std::ostream& os, const HitPoint& point);
