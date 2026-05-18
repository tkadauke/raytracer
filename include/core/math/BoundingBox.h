#pragma once

#include "core/InPlaceSetOperators.h"
#include "core/InequalityOperator.h"
#include "core/math/Range.h"
#include "core/math/Vector.h"
#include "core/math/Ray.h"
#include "render/Stats.h"

#include <functional>
#include <iostream>
#include <algorithm>

#ifdef __SSE2__
#include <emmintrin.h>
#endif

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
  * or grid data structure (see render::Grid).
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
    const T minX = min().x(), minY = min().y(), minZ = min().z();
    const T oMinX = other.min().x(), oMinY = other.min().y(), oMinZ = other.min().z();
    const T maxX = max().x(), maxY = max().y(), maxZ = max().z();
    const T oMaxX = other.max().x(), oMaxY = other.max().y(), oMaxZ = other.max().z();
    BoundingBox result(
      Vector3<T>(
        minX > oMinX ? minX : oMinX,
        minY > oMinY ? minY : oMinY,
        minZ > oMinZ ? minZ : oMinZ
      ),
      Vector3<T>(
        maxX < oMaxX ? maxX : oMaxX,
        maxY < oMaxY ? maxY : oMaxY,
        maxZ < oMaxZ ? maxZ : oMaxZ
      )
    );
    if (!result.isValid())
      return BoundingBox::undefined();
    return result;
  }
  
  /**
    * This function expands this bounding box, so that @p point will be inside
    * this box. The bounding box object is changed in place. The following
    * interactive figure illustrates the geometry. Drag the red point handle to
    * move the included point.
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
    *   @p vec. The following interactive figure illustrates the geometry. Drag
    *   the red top-right handle to change @p vec and the resulting box size.
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
    *   figure illustrates the geometry. Drag the red corner handle to change
    *   @p vec and move the resulting bounding box.
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

  /**
    * Tests whether @p ray intersects the bounding box, and if so populates
    * @p interval with the [t_enter, t_exit] slab intersection interval.
    * The interval is always written (even on a miss) so BVH callers can
    * use @p interval.begin() to prioritize child descent without recomputing
    * the entry distance.
    *
    * @param ray     The ray to test.
    * @param interval Output: the [t_enter, t_exit] interval along the ray.
    * @returns true if the ray hits the box (t_enter <= t_exit && t_exit >= 0).
    */
  bool intersect(const Rayd& ray, Range<T>& interval) const;

private:
  Vector3<T> m_min, m_max;
};

template<class T>
const BoundingBox<T>& BoundingBox<T>::undefined() {
  static BoundingBox<T> b(Vector3<T>::undefined(), Vector3<T>::undefined());
  return b;
}

template<class T>
const BoundingBox<T>& BoundingBox<T>::infinity() {
  static BoundingBox<T> b(Vector3<T>::minusInfinity(), Vector3<T>::plusInfinity());
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

// Generic branchless slab intersection — eliminates all per-axis sign branches
// by computing both (min-o)*inv_d and (max-o)*inv_d and using ternary min/max.
// Ternary operators return values (not references), avoiding GCC 13's
// -Wdangling-reference false positive on std::min/std::max chains.
// The compiler emits conditional-move instructions (no mispredicted branches).
// The SSE2 double explicit specialization follows below.
template<class T>
bool BoundingBox<T>::intersects(const Rayd& ray) const {
  RAYTRACER_STATS_INC(rayBoxIntersects);
  const T ox = ray.origin().x(), oy = ray.origin().y(), oz = ray.origin().z();
  const T invDx = T(1.0) / ray.direction().x();
  const T invDy = T(1.0) / ray.direction().y();
  const T invDz = T(1.0) / ray.direction().z();

  const T t1x = (m_min.x() - ox) * invDx, t2x = (m_max.x() - ox) * invDx;
  const T t1y = (m_min.y() - oy) * invDy, t2y = (m_max.y() - oy) * invDy;
  const T t1z = (m_min.z() - oz) * invDz, t2z = (m_max.z() - oz) * invDz;

  const T enter_x = t1x < t2x ? t1x : t2x;
  const T enter_y = t1y < t2y ? t1y : t2y;
  const T enter_z = t1z < t2z ? t1z : t2z;
  const T exit_x = t1x > t2x ? t1x : t2x;
  const T exit_y = t1y > t2y ? t1y : t2y;
  const T exit_z = t1z > t2z ? t1z : t2z;
  const T enter_xy = enter_x > enter_y ? enter_x : enter_y;
  const T t_enter = enter_xy > enter_z ? enter_xy : enter_z;
  const T exit_xy = exit_x < exit_y ? exit_x : exit_y;
  const T t_exit = exit_xy < exit_z ? exit_xy : exit_z;
  return t_enter <= t_exit && t_exit >= T(0.0);
}

template<class T>
bool BoundingBox<T>::intersect(const Rayd& ray, Range<T>& interval) const {
  RAYTRACER_STATS_INC(rayBoxIntersects);
  const T ox = ray.origin().x(), oy = ray.origin().y(), oz = ray.origin().z();
  const T invDx = T(1.0) / ray.direction().x();
  const T invDy = T(1.0) / ray.direction().y();
  const T invDz = T(1.0) / ray.direction().z();

  const T t1x = (m_min.x() - ox) * invDx, t2x = (m_max.x() - ox) * invDx;
  const T t1y = (m_min.y() - oy) * invDy, t2y = (m_max.y() - oy) * invDy;
  const T t1z = (m_min.z() - oz) * invDz, t2z = (m_max.z() - oz) * invDz;

  const T enter_x = t1x < t2x ? t1x : t2x;
  const T enter_y = t1y < t2y ? t1y : t2y;
  const T enter_z = t1z < t2z ? t1z : t2z;
  const T exit_x = t1x > t2x ? t1x : t2x;
  const T exit_y = t1y > t2y ? t1y : t2y;
  const T exit_z = t1z > t2z ? t1z : t2z;
  const T enter_xy = enter_x > enter_y ? enter_x : enter_y;
  const T t_enter = enter_xy > enter_z ? enter_xy : enter_z;
  const T exit_xy = exit_x < exit_y ? exit_x : exit_y;
  const T t_exit = exit_xy < exit_z ? exit_xy : exit_z;
  interval = Range<T>(t_enter, t_exit);
  return t_enter <= t_exit && t_exit >= T(0.0);
}


// ── SSE2 double specialization ───────────────────────────────────────────────
// Processes X and Y axes together in one __m128d pair; Z is scalar.
// The BVH uses BoundingBoxd (double) on every node per ray, so this is
// the hot path.
#ifdef __SSE2__
template<>
inline bool BoundingBox<double>::intersects(const Rayd& ray) const {
  RAYTRACER_STATS_INC(rayBoxIntersects);
  // X+Y: two lanes of __m128d — _mm_set_pd(high=y, low=x)
  const __m128d min_xy = _mm_set_pd(m_min.y(), m_min.x());
  const __m128d max_xy = _mm_set_pd(m_max.y(), m_max.x());
  const __m128d orig_xy = _mm_set_pd(ray.origin().y(), ray.origin().x());
  // Use _mm_div_pd so both reciprocals are computed in one SIMD instruction.
  const __m128d dir_xy = _mm_set_pd(ray.direction().y(), ray.direction().x());
  const __m128d invd_xy = _mm_div_pd(_mm_set1_pd(1.0), dir_xy);

  const __m128d t1_xy = _mm_mul_pd(_mm_sub_pd(min_xy, orig_xy), invd_xy);
  const __m128d t2_xy = _mm_mul_pd(_mm_sub_pd(max_xy, orig_xy), invd_xy);

  const __m128d enter_xy = _mm_min_pd(t1_xy, t2_xy);
  const __m128d exit_xy = _mm_max_pd(t1_xy, t2_xy);

  // Z axis — scalar; ternary min/max become conditional moves under -O3.
  // Ternary operators avoid -Wdangling-reference on std::min/std::max.
  const double invDz = 1.0 / ray.direction().z();
  const double t1z = (m_min.z() - ray.origin().z()) * invDz;
  const double t2z = (m_max.z() - ray.origin().z()) * invDz;
  const double enter_z = t1z < t2z ? t1z : t2z;
  const double exit_z = t1z > t2z ? t1z : t2z;

  // Horizontal reduce: max(enter_x, enter_y) then max with enter_z
  // _mm_unpackhi_pd([x,y],[x,y]) = [y, y]
  const __m128d enter_y = _mm_unpackhi_pd(enter_xy, enter_xy);
  const double t_enter_xy = _mm_cvtsd_f64(_mm_max_sd(enter_xy, enter_y));
  const double t_enter = t_enter_xy > enter_z ? t_enter_xy : enter_z;

  const __m128d exit_y = _mm_unpackhi_pd(exit_xy, exit_xy);
  const double t_exit_xy = _mm_cvtsd_f64(_mm_min_sd(exit_xy, exit_y));
  const double t_exit = t_exit_xy < exit_z ? t_exit_xy : exit_z;

  return t_enter <= t_exit && t_exit >= 0.0;
}

template<>
inline bool BoundingBox<double>::intersect(const Rayd& ray, Range<double>& interval) const {
  RAYTRACER_STATS_INC(rayBoxIntersects);
  const __m128d min_xy = _mm_set_pd(m_min.y(), m_min.x());
  const __m128d max_xy = _mm_set_pd(m_max.y(), m_max.x());
  const __m128d orig_xy = _mm_set_pd(ray.origin().y(), ray.origin().x());
  const __m128d dir_xy = _mm_set_pd(ray.direction().y(), ray.direction().x());
  const __m128d invd_xy = _mm_div_pd(_mm_set1_pd(1.0), dir_xy);

  const __m128d t1_xy = _mm_mul_pd(_mm_sub_pd(min_xy, orig_xy), invd_xy);
  const __m128d t2_xy = _mm_mul_pd(_mm_sub_pd(max_xy, orig_xy), invd_xy);

  const __m128d enter_xy = _mm_min_pd(t1_xy, t2_xy);
  const __m128d exit_xy = _mm_max_pd(t1_xy, t2_xy);

  const double invDz = 1.0 / ray.direction().z();
  const double t1z = (m_min.z() - ray.origin().z()) * invDz;
  const double t2z = (m_max.z() - ray.origin().z()) * invDz;
  const double enter_z = t1z < t2z ? t1z : t2z;
  const double exit_z = t1z > t2z ? t1z : t2z;

  const __m128d enter_y = _mm_unpackhi_pd(enter_xy, enter_xy);
  const double t_enter_xy = _mm_cvtsd_f64(_mm_max_sd(enter_xy, enter_y));
  const double t_enter = t_enter_xy > enter_z ? t_enter_xy : enter_z;

  const __m128d exit_y = _mm_unpackhi_pd(exit_xy, exit_xy);
  const double t_exit_xy = _mm_cvtsd_f64(_mm_min_sd(exit_xy, exit_y));
  const double t_exit = t_exit_xy < exit_z ? t_exit_xy : exit_z;

  interval = Range<double>(t_enter, t_exit);
  return t_enter <= t_exit && t_exit >= 0.0;
}
#endif  // __SSE2__


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

// ---------------------------------------------------------------------------
// std::hash specialization — enables unordered_map/unordered_set keys.
// ---------------------------------------------------------------------------
namespace std {  // NOLINT(cert-dcl58-cpp) — extending std for UDTs is allowed

  template<class T>
  struct hash<BoundingBox<T>> {
    size_t operator()(const BoundingBox<T>& b) const noexcept {
      size_t seed = hash<Vector3<T>>{}(b.min());
      seed ^= hash<Vector3<T>>{}(b.max()) + size_t(0x9e3779b9) + (seed << 6) + (seed >> 2);
      return seed;
    }
  };
}

// ---------------------------------------------------------------------------
// std::formatter specialization (C++20). Fallback: the operator<< above.
// ---------------------------------------------------------------------------
#ifdef __cpp_lib_format

template<class T>
struct std::formatter<BoundingBox<T>> {  // NOLINT(cert-dcl58-cpp)
  constexpr auto parse(std::format_parse_context& ctx) const { return ctx.begin(); }
  auto format(const BoundingBox<T>& b, std::format_context& ctx) const {
    return std::format_to(ctx.out(), "{}-{}", b.min(), b.max());
  }
};

#endif  // __cpp_lib_format
