#pragma once

#include "core/InPlaceSetOperators.h"
#include "core/InequalityOperator.h"
#include "core/math/Vector.h"
#include "core/math/Ray.h"

#include <iostream>
#include <algorithm>

/**
  * Represents a three-dimensional axis-aligned bounding box (AABB), a type of
  * [bounding volume](https://en.wikipedia.org/wiki/Bounding_volume). This class
  * supports ray intersection, bounding box intersection/union, as well as other
  * geometric operations. One purpose of this class is to help with optimized
  * rendering algorithms that quickly identify if a ray intersects the bounding
  * box of an object, therefore eliminating the need for computing a more
  * expenpensive intersection, when the ray misses the object. For this to work,
  * the bounding box should be as small as possible, while still containing the
  * entire object. Another purpose is to organize objects spatially in a tree
  * or grid data structure (see raytracer::Grid).
  * 
  * The bounding box is axis aligned, as illustrated in the following figure.
  * 
  * @htmlonly
  * <script type="text/javascript" src="figure.js"></script>
  * <script type="text/javascript" src="bounding_box_class.js"></script>
  * @endhtmlonly
  * 
  * The class inherits InPlaceSetOperators as well as InequalityOperator,
  * providing operators &=, |= and !=.
  * 
  * @tparam T The coordinate type.
  */
template<class T>
class BoundingBox
  : public InPlaceSetOperators<BoundingBox<T>>,
    public InequalityOperator<BoundingBox<T>>
{
public:
  /**
    * @returns the "undefined" bounding box.
    */
  static const BoundingBox<T>& undefined();
  
  /**
    * @returns an infinitely large bounding box.
    */
  static const BoundingBox<T>& infinity();
  
  /**
    * Default constructor. Creates an infinitely large bounding box.
    */
  inline BoundingBox()
    : m_min(Vector3<T>::plusInfinity()),
      m_max(Vector3<T>::minusInfinity())
  { 
  }

  /**
    * Creates a bounding box specified by the min and max corner vectors. A
    * bounding box is only valid if all components of @p min are smaller or
    * equal to their corresponding component of @p max. However, this
    * constructor does not check if the bounding box is valid.
    * 
    * @see isValid().
    */
  inline explicit BoundingBox(const Vector3<T>& min, const Vector3<T>& max)
    : m_min(min),
      m_max(max)
  {
  }
  
  /**
    * @returns true if the bounding box is valid, false otherwise. A bounding
    *   box is only valid if all components of min() are smaller than or equal
    *   to their corresponding component of max().
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
    return min().isUndefined() || max().isUndefined();
  }

  /**
    * @returns true if the bounding box is infintely large, false otherwise. A
    *   bounding box is infinitely large if at least one coordinate from any of
    *   the corner vectors is infinte.
    */
  inline bool isInfinite() const {
    return min().isInfinite() || max().isInfinite();
  }

  /**
    * @returns the smaller corner vector.
    */
  inline const Vector3<T>& min() const {
    return m_min;
  }
  
  /**
    * @returns the larger corner vector.
    */
  inline const Vector3<T>& max() const {
    return m_max;
  }
  
  /**
    * @returns the size of the bounding box, which is the difference of the
    *   max() and min() points.
    */
  inline Vector3<T> size() const {
    return max() - min();
  }
  
  /**
    * @returns the center point of the bounding box.
    */
  inline Vector3<T> center() const {
    return (min() + max()) * 0.5;
  }
  
  /**
    * @returns the width of the bounding box, i.e. the size along the X axis.
    */
  inline T width() const {
    return max().x() - min().x();
  }
  
  /**
    * @returns the height of the bounding box, i.e. the size along the Y axis.
    */
  inline T height() const {
    return max().y() - min().y();
  }
  
  /**
    * @returns the depth of the bounding box, i.e. the size along the Z axis.
    */
  inline T depth() const {
    return max().z() - min().z();
  }
  
  /**
    * @returns the volume of the bounding box.
    */
  inline T volume() const {
    return width() * height() * depth();
  }
  
  /**
    * @returns true if the volume of the bounding box is 0, false otherwise.
    */
  inline bool isEmpty() const {
    return volume() == 0;
  }
  
  /**
    * @returns true if this bounding box is equal to @p other, false otherwise.
    */
  inline bool operator==(const BoundingBox& other) const {
    if (this == &other)
      return true;
    return min() == other.min() && max() == other.max();
  }
  
  /**
    * Calculates the union of this bounding box and @p other. The union is by
    * definition at least as big as either one of the operands. The following
    * figure illustrates the geometry.
    * 
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="bounding_box_or.js"></script>
    * @endhtmlonly
    * 
    * @returns the smallest bouding box that contains both this and @p other.
    */
  inline BoundingBox operator|(const BoundingBox& other) const {
    BoundingBox result(*this);
    result.include(other);
    return result;
  }
  
  /**
    * Calculates the intersection of this bounding box and @p other. The
    * intersection is usually smaller than either one of the operands. The
    * following figure illustrates the geometry.
    * 
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="bounding_box_and.js"></script>
    * @endhtmlonly
    * 
    * @returns the smallest bounding box that contains all points that are both
    *   contained in this and in @p other.
    */
  inline BoundingBox operator&(const BoundingBox& other) const {
    BoundingBox result(
      Vector3<T>(
        std::max(min().x(), other.min().x()),
        std::max(min().y(), other.min().y()),
        std::max(min().z(), other.min().z())
      ),
      Vector3<T>(
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
    * This function expands this bounding box, so that @p point will be inside
    * this box. The bounding box object is changed in place. The following
    * interactive figure illustrates the geometry. Click and drag to move the
    * included point around, shown in red.
    * 
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="bounding_box_include.js"></script>
    * @endhtmlonly
    */
  inline void include(const Vector3<T>& point) {
    for (int i = 0; i != 3; ++i) {
      if (point[i] < m_min[i])
        m_min[i] = point[i];
      if (point[i] > m_max[i])
        m_max[i] = point[i];
    }
  }
  
  /**
    * This function expands the bounding box, so that every point of @p box will
    * be inside this bounding box.
    */
  inline void include(const BoundingBox<T>& box) {
    include(box.min());
    include(box.max());
  }
  
  /**
    * @returns true if and only if @p point is inside the box, false otherwise.
    */
  inline bool contains(const Vector3<T>& point) const {
    for (int i = 0; i != 3; ++i) {
      if (point[i] < m_min[i] || point[i] > m_max[i])
        return false;
    }
    return true;
  }
  
  /**
    * Alias for movedBy().
    */
  inline BoundingBox<T> operator+(const Vector3<T>& vec) const {
    return movedBy(vec);
  }
  
  /**
    * @returns a bounding box that is the Minkowski sum of this bounding box and
    *   @p bbox.
    */
  inline BoundingBox<T> operator+(const BoundingBox<T>& bbox) const {
    return BoundingBox<T>(min() + bbox.min(), max() + bbox.max());
  }
  
  /**
    * @returns a bounding box that is grown by @p vec in each direction. The
    *   resulting bounding box's size is the original size plus two times
    *   @p vec. The following interactive figure illustrates the geometry. Click
    *   and drag to change the size of the resulting bounding box. The red
    *   vector is @p vec.
    * 
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="bounding_box_grown_by.js"></script>
    * @endhtmlonly
    * 
    * Note that this method doesn't check whether the resulting bounding box is
    * valid. It can become invalid when @p vec has at least one negative
    * component that is larger than half of the corresponding component of the
    * size() vector.
    * 
    * @see valid().
    */
  inline BoundingBox<T> grownBy(const Vector3<T>& vec) const {
    return BoundingBox<T>(min() - vec, max() + vec);
  }
  
  /**
    * @returns a bounding box that is grown by \f$\epsilon\f$.
    */
  inline BoundingBox<T> grownByEpsilon() const {
    return grownBy(Vector3<T>::epsilon());
  }
  
  /**
    * @returns a bounding box that is moved by @p vec. The following interactive
    *   figure illustrates the geometry. Click and drag to move the resulting
    *   bounding box around.
    * 
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="bounding_box_moved_by.js"></script>
    * @endhtmlonly
    */
  inline BoundingBox<T> movedBy(const Vector3<T>& vec) const {
    return BoundingBox<T>(min() + vec, max() + vec);
  }
  
  /**
    * Adds the 8 corner vertices of the bounding box to the given generic
    * @p container.
    */
  template<class Container>
  void getVertices(Container& container) const;
  
  /**
    * @returns true, if and only if @p ray intersects with the bounding box.
    * Because the bounding box is likely to differ from the object contained
    * inside it, using this as an intersection prediction is not 100% accurate.
    * However, a Ray miss with the bounding box guarantees a Ray miss in the
    * contained object. Since a ray/box intersection is relatively cheap,
    * querying the bounding box first often leads to faster rendering times.
    */
  bool intersects(const Rayd& ray) const;
  
private:
  Vector3<T> m_min, m_max;
};

template<class T>
const BoundingBox<T>& BoundingBox<T>::undefined() {
  static BoundingBox<T> b(Vector3d::undefined(), Vector3d::undefined());
  return b;
}

template<class T>
const BoundingBox<T>& BoundingBox<T>::infinity() {
  static BoundingBox<T> b(Vector3d::minusInfinity(), Vector3d::plusInfinity());
  return b;
}

template<class T>
template<class Container>
void BoundingBox<T>::getVertices(Container& container) const {
  container.push_back(m_min);
  container.push_back(Vector3<T>(m_min.x(), m_min.y(), m_max.z()));
  container.push_back(Vector3<T>(m_min.x(), m_max.y(), m_min.z()));
  container.push_back(Vector3<T>(m_min.x(), m_max.y(), m_max.z()));
  container.push_back(Vector3<T>(m_max.x(), m_min.y(), m_min.z()));
  container.push_back(Vector3<T>(m_max.x(), m_min.y(), m_max.z()));
  container.push_back(Vector3<T>(m_max.x(), m_max.y(), m_min.z()));
  container.push_back(m_max);
}

template<class T>
bool BoundingBox<T>::intersects(const Rayd& ray) const {
  T ox = ray.origin().x();
  T oy = ray.origin().y();
  T oz = ray.origin().z();
  T dx = ray.direction().x();
  T dy = ray.direction().y();
  T dz = ray.direction().z();
  
  T xMin, yMin, zMin;
  T xMax, yMax, zMax;
  
  T a = 1.0 / dx;
  if (a >= 0) {
    xMin = (m_min.x() - ox) * a;
    xMax = (m_max.x() - ox) * a;
  } else {
    xMin = (m_max.x() - ox) * a;
    xMax = (m_min.x() - ox) * a;
  }
  
  T b = 1.0 / dy;
  if (b >= 0) {
    yMin = (m_min.y() - oy) * b;
    yMax = (m_max.y() - oy) * b;
  } else {
    yMin = (m_max.y() - oy) * b;
    yMax = (m_min.y() - oy) * b;
  }
  
  T c = 1.0 / dz;
  if (c >= 0) {
    zMin = (m_min.z() - oz) * c;
    zMax = (m_max.z() - oz) * c;
  } else {
    zMin = (m_max.z() - oz) * c;
    zMax = (m_min.z() - oz) * c;
  }
  
  T t0, t1;
  
  if (xMin > yMin)
    t0 = xMin;
  else
    t0 = yMin;
  
  if (zMin > t0)
    t0 = zMin;
  
  if (xMax < yMax)
    t1 = xMax;
  else
    t1 = yMax;
  
  if (zMax < t1)
    t1 = zMax;
  
  return (t0 <= t1 && t1 >= 0.0);
}


/**
  * Outputs the given bounding box to the given std::ostream.
  * 
  * @returns @p os.
  */
template<class T>
inline std::ostream& operator<<(std::ostream& os, const BoundingBox<T>& bbox) {
  return os << bbox.min() << "-" << bbox.max();
}

/**
  * Shortcut for bounding box with float precision.
  */
typedef BoundingBox<float> BoundingBoxf;

/**
  * Shortcut for bounding box with double precision.
  */
typedef BoundingBox<double> BoundingBoxd;
