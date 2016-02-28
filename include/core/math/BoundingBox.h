#pragma once

#include "core/InPlaceSetOperators.h"
#include "core/InequalityOperator.h"
#include "core/math/Vector.h"

#include <iostream>
#include <algorithm>

class Ray;

/**
  * Represents a three-dimensional axis-aligned bounding box (AABB), a type of
  * [bounding volume](https://en.wikipedia.org/wiki/Bounding_volume). This class
  * supports ray intersection, bounding box intersection/union, as well as other
  * geometric operation. The purpose of this class is to help with optimized
  * rendering algorithms that quickly identify if a ray intersects the bounding
  * box.
  * 
  * The class inherits InPlaceSetOperators as well as InequalityOperator,
  * providing operators &=, |= and !=.
  */
class BoundingBox
  : public InPlaceSetOperators<BoundingBox>,
    public InequalityOperator<BoundingBox>
{
public:
  /**
    * Represents the "undefined" bounding box.
    */
  static const BoundingBox& undefined();
  
  /**
    * Creates an infinitely large bounding box.
    */
  inline BoundingBox()
    : m_min(Vector3d::plusInfinity()),
      m_max(Vector3d::minusInfinity())
  { 
  }

  /**
    * Creates a bounding box specified by the min and max corner vectors. A
    * bounding box is only valid if all components of min are smaller or equal
    * to their corresponding component of max.
    */
  inline BoundingBox(const Vector3d& min, const Vector3d& max)
    : m_min(min),
      m_max(max)
  {
  }
  
  /**
    * @returns true if the bounding box is valid, false otherwise. A bounding
    *   box is only valid if all components of min are smaller than or equal to
    *   their corresponding component of max.
    */
  inline bool isValid() const {
    return min().x() <= max().x() &&
           min().y() <= max().y() &&
           min().z() <= max().z();
  }

  /**
    * @returns true if the bounding box is undefined, false otherwise. A
    *   bounding box is undefined if either of the corner vectors is undefined.
    */
  inline bool isUndefined() const {
    return m_min.isUndefined() || m_max.isUndefined();
  }

  /**
    * @returns the smaller corner vector.
    */
  inline const Vector3d& min() const {
    return m_min;
  }
  
  /**
    * @returns the larger corner vector.
    */
  inline const Vector3d& max() const {
    return m_max;
  }
  
  /**
    * @returns the center point of the bounding box.
    */
  inline Vector3d center() const {
    return (m_min + m_max) * 0.5;
  }
  
  /**
    * @returns true if this bounding box is equal to other, false otherwise.
    */
  inline bool operator==(const BoundingBox& other) const {
    if (this == &other)
      return true;
    return min() == other.min() && max() == other.max();
  }
  
  /**
    * Calculates the union of this bounding box and other. The union is by
    * definition at least as big as either one of the operands.
    * 
    * @returns the smallest BoundingBox that contains both this and other.
    */
  inline BoundingBox operator|(const BoundingBox& other) const {
    BoundingBox result(*this);
    result.include(other);
    return result;
  }
  
  /**
    * Calculates the intersection of this bounding box and other. The
    * intersection is usually smaller than either one of the operands.
    * 
    * @returns the smallest BoundingBox that contains all points that are both
    *   contained in this and in other.
    */
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
      return BoundingBox::undefined();
    return result;
  }
  
  /**
    * This function expands this bounding box, so that point will be inside this
    * box.
    */
  inline void include(const Vector3d& point) {
    for (int i = 0; i != 3; ++i) {
      if (point[i] < m_min[i])
        m_min[i] = point[i];
      if (point[i] > m_max[i])
        m_max[i] = point[i];
    }
  }
  
  /**
    * This function expands the bounding box, so that every point of box will be
    * inside this bounding box.
    */
  inline void include(const BoundingBox& box) {
    include(box.min());
    include(box.max());
  }
  
  /**
    * @returns true if and only if point is inside the box, false otherwise.
    */
  inline bool contains(const Vector3d& point) {
    for (int i = 0; i != 3; ++i) {
      if (point[i] < m_min[i] || point[i] > m_max[i])
        return false;
    }
    return true;
  }
  
  /**
    * Adds the 8 corner vertices of the bounding box to the given generic
    * container.
    */
  template<class Container>
  void getVertices(Container& container);
  
  /**
    * This is probably the most important method of this class. It returns
    * true, if and only if ray intersects with the current object.
    * Because the bounding box is likely to differ from the actual object, this
    * intersection prediction is not 100% accurate. However, a Ray miss in the
    * bounding box guarantees a Ray miss in the rendering object. Since a
    * ray/box intersection is relatively cheap, querying the bounding box first
    * often leads to faster rendering times.
    * 
    * @returns true if ray intersects with this BoundingBox, false otherwise.
    */
  bool intersects(const Ray& ray) const;
  
private:
  Vector3d m_min, m_max;
};

template<class Container>
void BoundingBox::getVertices(Container& container) {
  container.push_back(m_min);
  container.push_back(Vector3d(m_min.x(), m_min.y(), m_max.z()));
  container.push_back(Vector3d(m_min.x(), m_max.y(), m_min.z()));
  container.push_back(Vector3d(m_min.x(), m_max.y(), m_max.z()));
  container.push_back(Vector3d(m_max.x(), m_min.y(), m_min.z()));
  container.push_back(Vector3d(m_max.x(), m_min.y(), m_max.z()));
  container.push_back(Vector3d(m_max.x(), m_max.y(), m_min.z()));
  container.push_back(m_max);
}


/**
  * Outputs the given bounding box to the given std::ostream.
  * 
  * @returns os.
  */
inline std::ostream& operator<<(std::ostream& os, const BoundingBox& bbox) {
  return os << bbox.min() << "-" << bbox.max();
}
