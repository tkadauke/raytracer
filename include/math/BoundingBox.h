#ifndef BOUNDING_BOX_H
#define BOUNDING_BOX_H

#include "core/InPlaceSetOperators.h"
#include "core/InequalityOperator.h"
#include "math/Vector.h"

#include <iostream>
#include <algorithm>

class BoundingBox : public InPlaceSetOperators<BoundingBox>, public InequalityOperator<BoundingBox> {
public:
  static const BoundingBox undefined;
  
  BoundingBox()
    : m_min(Vector3d::plusInfinity), m_max(Vector3d::minusInfinity) {}
  BoundingBox(const Vector3d& min, const Vector3d& max)
    : m_min(min), m_max(max) {}
  
  inline const Vector3d& min() const { return m_min; }
  inline const Vector3d& max() const { return m_max; }
  inline Vector3d center() const { return (m_min + m_max) * 0.5; }
  
  inline bool operator==(const BoundingBox& other) const {
    if (this == &other)
      return true;
    return min() == other.min() && max() == other.max();
  }
  
  inline BoundingBox operator|(const BoundingBox& other) const {
    BoundingBox result(*this);
    result.include(other);
    return result;
  }
  
  inline BoundingBox operator&(const BoundingBox& other) const {
    BoundingBox result(
      Vector3d(
        std::max(min().x(), other.min().x()),
        std::max(min().y(), other.min().y()),
        std::max(min().z(), other.min().z())
      ),
      Vector3d(
        std::min(max().x(), other.max().x()),
        std::min(max().y(), other.max().y()),
        std::min(max().z(), other.max().z())
      )
    );
    if (!result.isValid())
      return BoundingBox::undefined;
    return result;
  }
  
  inline void include(const Vector3d& point) {
    for (int i = 0; i != 3; ++i) {
      if (point[i] < m_min[i])
        m_min[i] = point[i];
      if (point[i] > m_max[i])
        m_max[i] = point[i];
    }
  }
  
  inline void include(const BoundingBox& box) {
    include(box.min());
    include(box.max());
  }
  
  template<class Container>
  void getVertices(Container& container) {
    container.push_back(m_min);
    container.push_back(Vector3d(m_min.x(), m_min.y(), m_max.z()));
    container.push_back(Vector3d(m_min.x(), m_max.y(), m_min.z()));
    container.push_back(Vector3d(m_min.x(), m_max.y(), m_max.z()));
    container.push_back(Vector3d(m_max.x(), m_min.y(), m_min.z()));
    container.push_back(Vector3d(m_max.x(), m_min.y(), m_max.z()));
    container.push_back(Vector3d(m_max.x(), m_max.y(), m_min.z()));
    container.push_back(m_max);
  }
  
  inline bool isValid() const {
    return min().x() < max().x() &&
           min().y() < max().y() &&
           min().z() < max().z();
  }
  
  inline bool isUndefined() const {
    return min().isUndefined() || max().isUndefined();
  }
  
private:
  Vector3d m_min, m_max;
};

inline std::ostream& operator<<(std::ostream& os, const BoundingBox& bbox) {
  return os << '(' << bbox.min() << ")-(" << bbox.max() << ')';
}

#endif
