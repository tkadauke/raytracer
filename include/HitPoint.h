#ifndef HIT_POINT_H
#define HIT_POINT_H

#include "Vector.h"
#include <cmath>

class HitPoint {
public:
  static const HitPoint undefined;
  
  inline HitPoint() : m_distance(0) {}
  
  inline HitPoint(double distance, const Vector3d& point, const Vector3d& normal)
    : m_distance(distance), m_point(point), m_normal(normal)
  {
  }
  
  inline double distance() const { return m_distance; }
  inline void setDistance(double distance) { m_distance = distance; }
  
  inline const Vector3d& point() const { return m_point; }
  inline void setPoint(const Vector3d& point) { m_point = point; }
  
  inline const Vector3d& normal() const { return m_normal; }
  inline void setNormal(const Vector3d& normal) { m_normal = normal; }
  
  inline HitPoint swappedNormal() const { return HitPoint(m_distance, m_point, -m_normal); }
  
  inline bool operator==(const HitPoint& other) const {
    if (&other == this)
      return true;
    return m_distance == other.distance() &&
           m_point == other.point() &&
           m_normal == other.normal();
  }
  
  inline bool isUndefined() const { return std::isnan(m_distance) || m_point.isUndefined() || m_normal.isUndefined(); }
  
private:
  double m_distance;
  Vector3d m_point, m_normal;
};

#endif
