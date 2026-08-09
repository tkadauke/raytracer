#pragma once

#include "core/SimdFeatures.h"

#include "core/InPlaceSetOperators.h"
#include "core/InequalityOperator.h"
#include "core/math/Range.h"
#include "core/math/Vector.h"
#include "core/math/Ray.h"
#include "core/math/RayPacket.h"
#include "core/simd/Float4.h"
#include "render/Stats.h"

#include <array>
#include <functional>
#include <iostream>
#include <algorithm>

#if RAYTRACER_SIMD_SSE2
#include <emmintrin.h>
#endif
#if RAYTRACER_SIMD_SSE
#include <xmmintrin.h>
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
class BoundingBox : public InPlaceSetOperators<BoundingBox<T>>,
                    public InequalityOperator<BoundingBox<T>> {
public:
  /**
    * The "undefined" bounding box.
    */
  static const BoundingBox<T> undefined;

  /**
    * An infinitely large bounding box.
    */
  static const BoundingBox<T> infinity;

  /**
    * Default constructor. Creates an infinitely large bounding box.
    */
  inline BoundingBox()
      : m_min(Vector3<T>::plusInfinity),
        m_max(Vector3<T>::minusInfinity) {
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
        m_max(max) {
  }

  /**
    * @returns true if the bounding box is valid, false otherwise. A bounding
    *   box is only valid if all components of min() are smaller than or equal
    *   to their corresponding component of max().
    */
  [[nodiscard]] inline bool isValid() const noexcept {
    return min().x() <= max().x() && min().y() <= max().y() && min().z() <= max().z();
  }

  /**
    * @returns true if the bounding box is undefined, false otherwise. A
    *   bounding box is undefined if either of the corner vectors is undefined.
    */
  [[nodiscard]] inline bool isUndefined() const noexcept {
    return min().isUndefined() || max().isUndefined();
  }

  /**
    * @returns true if the bounding box is infintely large, false otherwise. A
    *   bounding box is infinitely large if at least one coordinate from any of
    *   the corner vectors is infinte.
    */
  [[nodiscard]] inline bool isInfinite() const noexcept {
    return min().isInfinite() || max().isInfinite();
  }

  /**
    * @returns the smaller corner vector.
    */
  [[nodiscard]] inline const Vector3<T>& min() const noexcept {
    return m_min;
  }

  /**
    * @returns the larger corner vector.
    */
  [[nodiscard]] inline const Vector3<T>& max() const noexcept {
    return m_max;
  }

  /**
    * @returns the size of the bounding box, which is the difference of the
    *   max() and min() points.
    */
  [[nodiscard]] inline Vector3<T> size() const noexcept {
    return max() - min();
  }

  /**
    * @returns the center point of the bounding box.
    */
  [[nodiscard]] inline Vector3<T> center() const noexcept {
    return (min() + max()) * 0.5;
  }

  /**
    * @returns the width of the bounding box, i.e. the size along the X axis.
    */
  [[nodiscard]] inline T width() const noexcept {
    return max().x() - min().x();
  }

  /**
    * @returns the height of the bounding box, i.e. the size along the Y axis.
    */
  [[nodiscard]] inline T height() const noexcept {
    return max().y() - min().y();
  }

  /**
    * @returns the depth of the bounding box, i.e. the size along the Z axis.
    */
  [[nodiscard]] inline T depth() const noexcept {
    return max().z() - min().z();
  }

  /**
    * @returns the volume of the bounding box.
    */
  [[nodiscard]] inline T volume() const noexcept {
    return width() * height() * depth();
  }

  /**
    * @returns the surface area of the bounding box, which is 2 * (xy + yz + zx).
    *   Returns 0 for an invalid bounding box. This is used as the cost estimate
    *   in the Surface Area Heuristic for BVH construction.
    */
  [[nodiscard]] inline T surfaceArea() const noexcept {
    if (!isValid())
      return T(0);
    const auto s = size();
    return T(2) * (s.x() * s.y() + s.y() * s.z() + s.z() * s.x());
  }

  /**
    * @returns true if the volume of the bounding box is 0, false otherwise.
    */
  [[nodiscard]] inline bool isEmpty() const noexcept {
    return volume() == 0;
  }

  /**
    * @returns true if this bounding box is equal to @p other, false otherwise.
    */
  [[nodiscard]] inline bool operator==(const BoundingBox& other) const noexcept {
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
  [[nodiscard]] inline BoundingBox operator|(const BoundingBox& other) const noexcept {
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
  [[nodiscard]] inline BoundingBox operator&(const BoundingBox& other) const noexcept {
    const T minX = min().x(), minY = min().y(), minZ = min().z();
    const T oMinX = other.min().x(), oMinY = other.min().y(), oMinZ = other.min().z();
    const T maxX = max().x(), maxY = max().y(), maxZ = max().z();
    const T oMaxX = other.max().x(), oMaxY = other.max().y(), oMaxZ = other.max().z();
    BoundingBox result(Vector3<T>(minX > oMinX ? minX : oMinX, minY > oMinY ? minY : oMinY,
                                  minZ > oMinZ ? minZ : oMinZ),
                       Vector3<T>(maxX < oMaxX ? maxX : oMaxX, maxY < oMaxY ? maxY : oMaxY,
                                  maxZ < oMaxZ ? maxZ : oMaxZ));
    if (!result.isValid())
      return BoundingBox::undefined;
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
  [[nodiscard]] inline bool contains(const Vector3<T>& point) const noexcept {
    for (int i = 0; i != 3; ++i) {
      if (point[i] < m_min[i] || point[i] > m_max[i])
        return false;
    }
    return true;
  }

  /**
    * Alias for movedBy().
    */
  [[nodiscard]] inline BoundingBox<T> operator+(const Vector3<T>& vec) const noexcept {
    return movedBy(vec);
  }

  /**
    * @returns a bounding box that is the Minkowski sum of this bounding box and
    *   @p bbox.
    */
  [[nodiscard]] inline BoundingBox<T> operator+(const BoundingBox<T>& bbox) const noexcept {
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
  [[nodiscard]] inline BoundingBox<T> grownBy(const Vector3<T>& vec) const noexcept {
    return BoundingBox<T>(min() - vec, max() + vec);
  }

  /**
    * @returns a bounding box that is grown by \f$\epsilon\f$.
    */
  [[nodiscard]] inline BoundingBox<T> grownByEpsilon() const noexcept {
    return grownBy(Vector3<T>::epsilon);
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
  [[nodiscard]] inline BoundingBox<T> movedBy(const Vector3<T>& vec) const noexcept {
    return BoundingBox<T>(min() + vec, max() + vec);
  }

  /**
    * @returns the 8 corner vertices of the bounding box.
    */
  [[nodiscard]] std::array<Vector3<T>, 8> vertices() const;

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
    * Variant of intersects() for acceleration structures that test the same
    * ray against many boxes. @p inverseDirection must contain the component-
    * wise reciprocal of @p ray.direction().
    */
  bool intersects(const Rayd& ray, const Vector3<T>& inverseDirection) const;

  /**
    * Tests four rays against this bounding box and returns one hit bit per
    * SIMD lane. Use `core::simd::movemask(result)` to extract the lane mask.
    */
  core::simd::Mask4 intersects4(const Ray4& rays) const;

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

  /**
    * Variant of intersect() for acceleration structures that test the same
    * ray against many boxes. @p inverseDirection must contain the component-
    * wise reciprocal of @p ray.direction().
    */
  bool intersect(const Rayd& ray, const Vector3<T>& inverseDirection, Range<T>& interval) const;

private:
  Vector3<T> m_min, m_max;
};

template<class T>
inline const BoundingBox<T> BoundingBox<T>::undefined{
  Vector3<T>(std::numeric_limits<T>::quiet_NaN(), std::numeric_limits<T>::quiet_NaN(),
             std::numeric_limits<T>::quiet_NaN()),
  Vector3<T>(std::numeric_limits<T>::quiet_NaN(), std::numeric_limits<T>::quiet_NaN(),
             std::numeric_limits<T>::quiet_NaN())};

template<class T>
inline const BoundingBox<T> BoundingBox<T>::infinity{
  Vector3<T>(-std::numeric_limits<T>::infinity(), -std::numeric_limits<T>::infinity(),
             -std::numeric_limits<T>::infinity()),
  Vector3<T>(std::numeric_limits<T>::infinity(), std::numeric_limits<T>::infinity(),
             std::numeric_limits<T>::infinity())};

template<class T>
template<class Container>
void BoundingBox<T>::getVertices(Container& container) const {
  for (const auto& vertex : vertices())
    container.push_back(vertex);
}

template<class T>
std::array<Vector3<T>, 8> BoundingBox<T>::vertices() const {
  return {{
    m_min,
    Vector3<T>(m_min.x(), m_min.y(), m_max.z()),
    Vector3<T>(m_min.x(), m_max.y(), m_min.z()),
    Vector3<T>(m_min.x(), m_max.y(), m_max.z()),
    Vector3<T>(m_max.x(), m_min.y(), m_min.z()),
    Vector3<T>(m_max.x(), m_min.y(), m_max.z()),
    Vector3<T>(m_max.x(), m_max.y(), m_min.z()),
    m_max,
  }};
}

#if RAYTRACER_SIMD_SSE2
template<>
inline bool BoundingBox<double>::intersect(const Rayd& ray, const Vector3<double>& inverseDirection,
                                           Range<double>& interval) const;
template<>
inline bool BoundingBox<double>::intersects(const Rayd& ray,
                                            const Vector3<double>& inverseDirection) const;
#endif

// Generic branchless slab intersection — eliminates all per-axis sign branches
// by computing both (min-o)*inv_d and (max-o)*inv_d and using ternary min/max.
// Ternary operators return values (not references), avoiding GCC 13's
// -Wdangling-reference false positive on std::min/std::max chains.
// The compiler emits conditional-move instructions (no mispredicted branches).
// The SSE2 double explicit specialization follows below.
template<class T>
bool BoundingBox<T>::intersects(const Rayd& ray) const {
  return intersects(ray, Vector3<T>(T(1.0) / ray.direction().x(), T(1.0) / ray.direction().y(),
                                    T(1.0) / ray.direction().z()));
}

template<class T>
bool BoundingBox<T>::intersects(const Rayd& ray, const Vector3<T>& inverseDirection) const {
  RAYTRACER_STATS_INC(rayBoxIntersects);
  const T ox = ray.origin().x(), oy = ray.origin().y(), oz = ray.origin().z();
  const T invDx = inverseDirection.x();
  const T invDy = inverseDirection.y();
  const T invDz = inverseDirection.z();

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
  return intersect(ray,
                   Vector3<T>(T(1.0) / ray.direction().x(), T(1.0) / ray.direction().y(),
                              T(1.0) / ray.direction().z()),
                   interval);
}

template<class T>
bool BoundingBox<T>::intersect(const Rayd& ray, const Vector3<T>& inverseDirection,
                               Range<T>& interval) const {
  RAYTRACER_STATS_INC(rayBoxIntersects);
  const T ox = ray.origin().x(), oy = ray.origin().y(), oz = ray.origin().z();
  const T invDx = inverseDirection.x();
  const T invDy = inverseDirection.y();
  const T invDz = inverseDirection.z();

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

template<class T>
inline __attribute__((always_inline)) core::simd::Mask4
BoundingBox<T>::intersects4(const Ray4& rays) const {
  RAYTRACER_STATS_INC(rayBoxIntersects);

  using namespace core::simd;

  const Float4 zeroValue = zero();
  const Float4 ox = load4(rays.originX.data());
  const Float4 oy = load4(rays.originY.data());
  const Float4 oz = load4(rays.originZ.data());
  const Float4 dx = load4(rays.directionX.data());
  const Float4 dy = load4(rays.directionY.data());
  const Float4 dz = load4(rays.directionZ.data());
  const Float4 minX = set1(static_cast<float>(m_min.x()));
  const Float4 minY = set1(static_cast<float>(m_min.y()));
  const Float4 minZ = set1(static_cast<float>(m_min.z()));
  const Float4 maxX = set1(static_cast<float>(m_max.x()));
  const Float4 maxY = set1(static_cast<float>(m_max.y()));
  const Float4 maxZ = set1(static_cast<float>(m_max.z()));

  const int zeroDirectionMask =
    movemask(maskOr(maskOr(cmpEq(dx, zeroValue), cmpEq(dy, zeroValue)), cmpEq(dz, zeroValue)));
  if (zeroDirectionMask == 0) {
    const Float4 one = set1(1.0f);
    const Float4 invDx = one / dx;
    const Float4 invDy = one / dy;
    const Float4 invDz = one / dz;
    const Float4 t1x = (minX - ox) * invDx;
    const Float4 t2x = (maxX - ox) * invDx;
    const Float4 t1y = (minY - oy) * invDy;
    const Float4 t2y = (maxY - oy) * invDy;
    const Float4 t1z = (minZ - oz) * invDz;
    const Float4 t2z = (maxZ - oz) * invDz;
    const Float4 enter =
      core::simd::max(core::simd::max(core::simd::min(t1x, t2x), core::simd::min(t1y, t2y)),
                      core::simd::min(t1z, t2z));
    const Float4 exit =
      core::simd::min(core::simd::min(core::simd::max(t1x, t2x), core::simd::max(t1y, t2y)),
                      core::simd::max(t1z, t2z));
    return maskAnd(cmpLe(enter, exit), cmpGe(exit, zeroValue));
  }

  const Float4 one = set1(1.0f);
  const Float4 negInfinity = set1(-std::numeric_limits<float>::infinity());
  const Float4 posInfinity = set1(std::numeric_limits<float>::infinity());

  auto axis = [&](const Ray4::LaneArray& origins, const Ray4::LaneArray& directions, float minValue,
                  float maxValue, Float4& enter, Float4& exit, Mask4& valid) {
    const Float4 o = load4(origins.data());
    const Float4 d = load4(directions.data());
    const Float4 minv = set1(minValue);
    const Float4 maxv = set1(maxValue);
    const Mask4 parallel = cmpEq(d, zeroValue);
    const Mask4 inside = maskAnd(cmpGe(o, minv), cmpLe(o, maxv));
    const Float4 invD = one / d;
    const Float4 t1 = (minv - o) * invD;
    const Float4 t2 = (maxv - o) * invD;
    const Float4 axisEnter = core::simd::min(t1, t2);
    const Float4 axisExit = core::simd::max(t1, t2);

    enter = core::simd::max(enter, select(parallel, negInfinity, axisEnter));
    exit = core::simd::min(exit, select(parallel, posInfinity, axisExit));
    valid = maskAnd(valid, maskOr(maskAndNot(parallel, cmpEq(d, d)), maskAnd(parallel, inside)));
  };

  Float4 enter = negInfinity;
  Float4 exit = posInfinity;
  Mask4 valid = cmpEq(zeroValue, zeroValue);
  axis(rays.originX, rays.directionX, static_cast<float>(m_min.x()), static_cast<float>(m_max.x()),
       enter, exit, valid);
  axis(rays.originY, rays.directionY, static_cast<float>(m_min.y()), static_cast<float>(m_max.y()),
       enter, exit, valid);
  axis(rays.originZ, rays.directionZ, static_cast<float>(m_min.z()), static_cast<float>(m_max.z()),
       enter, exit, valid);

  return maskAnd(valid, maskAnd(cmpLe(enter, exit), cmpGe(exit, zeroValue)));
}

// ── SSE2 double specialization ───────────────────────────────────────────────
// Processes X and Y axes together in one __m128d pair; Z is scalar.
// The BVH uses BoundingBoxd (double) on every node per ray, so this is
// the hot path.
#if RAYTRACER_SIMD_SSE2
template<>
inline bool BoundingBox<double>::intersects(const Rayd& ray) const {
  return intersects(
    ray, Vector3d(1.0 / ray.direction().x(), 1.0 / ray.direction().y(), 1.0 / ray.direction().z()));
}

template<>
inline bool BoundingBox<double>::intersects(const Rayd& ray,
                                            const Vector3<double>& inverseDirection) const {
  RAYTRACER_STATS_INC(rayBoxIntersects);
  // X+Y: two lanes of __m128d — _mm_set_pd(high=y, low=x)
  const __m128d min_xy = _mm_set_pd(m_min.y(), m_min.x());
  const __m128d max_xy = _mm_set_pd(m_max.y(), m_max.x());
  const __m128d orig_xy = _mm_set_pd(ray.origin().y(), ray.origin().x());
  const __m128d invd_xy = _mm_set_pd(inverseDirection.y(), inverseDirection.x());

  const __m128d t1_xy = _mm_mul_pd(_mm_sub_pd(min_xy, orig_xy), invd_xy);
  const __m128d t2_xy = _mm_mul_pd(_mm_sub_pd(max_xy, orig_xy), invd_xy);

  const __m128d enter_xy = _mm_min_pd(t1_xy, t2_xy);
  const __m128d exit_xy = _mm_max_pd(t1_xy, t2_xy);

  // Z axis — scalar; ternary min/max become conditional moves under -O3.
  // Ternary operators avoid -Wdangling-reference on std::min/std::max.
  const double invDz = inverseDirection.z();
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
  return intersect(
    ray, Vector3d(1.0 / ray.direction().x(), 1.0 / ray.direction().y(), 1.0 / ray.direction().z()),
    interval);
}

template<>
inline bool BoundingBox<double>::intersect(const Rayd& ray, const Vector3<double>& inverseDirection,
                                           Range<double>& interval) const {
  RAYTRACER_STATS_INC(rayBoxIntersects);
  const __m128d min_xy = _mm_set_pd(m_min.y(), m_min.x());
  const __m128d max_xy = _mm_set_pd(m_max.y(), m_max.x());
  const __m128d orig_xy = _mm_set_pd(ray.origin().y(), ray.origin().x());
  const __m128d invd_xy = _mm_set_pd(inverseDirection.y(), inverseDirection.x());

  const __m128d t1_xy = _mm_mul_pd(_mm_sub_pd(min_xy, orig_xy), invd_xy);
  const __m128d t2_xy = _mm_mul_pd(_mm_sub_pd(max_xy, orig_xy), invd_xy);

  const __m128d enter_xy = _mm_min_pd(t1_xy, t2_xy);
  const __m128d exit_xy = _mm_max_pd(t1_xy, t2_xy);

  const double invDz = inverseDirection.z();
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
#endif // RAYTRACER_SIMD_SSE2

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
namespace std { // NOLINT(cert-dcl58-cpp) — extending std for UDTs is allowed

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
struct std::formatter<BoundingBox<T>> { // NOLINT(cert-dcl58-cpp)
  constexpr auto parse(std::format_parse_context& ctx) const {
    return ctx.begin();
  }
  auto format(const BoundingBox<T>& b, std::format_context& ctx) const {
    return std::format_to(ctx.out(), "{}-{}", b.min(), b.max());
  }
};

#endif // __cpp_lib_format
