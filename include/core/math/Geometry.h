#pragma once

#include <cassert>
#include <cmath>
#include <type_traits>

#include "core/math/Matrix.h"
#include "core/math/Vector.h"

/**
  * @file
  * Semantic geometry types: Point3, Direction3 and Normal3, plus the
  * NormalMatrix3 inverse-transpose companion. The existing Vector3 is the
  * free-form displacement type.
  *
  * The types make geometric intent part of the type system:
  *
  * - `Point + Point` and `Point * scalar` do not compile.
  * - `Point - Point` is a Vector3; `Point +/- Vector3` is a Point.
  * - Direction3 and Normal3 are unit length. Arithmetic on them yields a
  *   Vector3 (the result is no longer unit length), and a Vector3 only
  *   becomes a Direction3 / Normal3 through toDirection() / toNormal().
  * - Matrix4 transforms Points with translation and Directions without.
  *   `Matrix * Normal3` is deleted; normals go through NormalMatrix3, which
  *   holds the inverse-transpose of the linear part.
  * - In builds with assertions enabled (no NDEBUG), constructing a Point3
  *   with w != 1 or a Direction3 / Normal3 that is not unit length trips an
  *   assertion. Define RT_GEOM_ASSERT before including this header to
  *   override, e.g. to compile the checks out in an assert-enabled build.
  */

#ifndef RT_GEOM_ASSERT
#define RT_GEOM_ASSERT(cond) assert(cond)
#endif

namespace geom {
  /** Tolerance on |w - 1| for Point3 (looser: homogeneous round trips drift). */
  template<class T>
  inline constexpr T pointWTolerance = T(1e-5);

  /** Tolerance on | |v|^2 - 1 | for Direction3 / Normal3 (squared, no sqrt). */
  template<class T>
  inline constexpr T unitLengthTolerance = T(1e-4);

  template<class T>
  [[nodiscard]] inline bool isUnitLength(const Vector3<T>& v) noexcept {
    return std::abs(v.squaredLength() - T(1)) <= unitLengthTolerance<T>;
  }
}

/**
  * A position in 3D space. Stores its homogeneous w explicitly (always 1 for a
  * valid point), so a 4x4 matrix multiply applies translation naturally.
  */
template<class T>
class Point3 {
public:
  inline constexpr Point3() noexcept = default;

  inline Point3(const T& x, const T& y, const T& z, const T& w = T(1))
      : m_xyz(x, y, z), m_w(w) {
    checkInvariant();
  }

  /** Explicit: a free-form vector is only a position when the caller says so. */
  inline explicit Point3(const Vector3<T>& position)
      : m_xyz(position), m_w(T(1)) {
  }

  [[nodiscard]] inline T x() const noexcept { return m_xyz.x(); }
  [[nodiscard]] inline T y() const noexcept { return m_xyz.y(); }
  [[nodiscard]] inline T z() const noexcept { return m_xyz.z(); }
  [[nodiscard]] inline T w() const noexcept { return m_w; }

  /** The position as a vector from the origin. */
  [[nodiscard]] inline const Vector3<T>& toVector() const noexcept { return m_xyz; }

  [[nodiscard]] inline bool operator==(const Point3& other) const noexcept {
    return m_xyz == other.m_xyz && m_w == other.m_w;
  }

  [[nodiscard]] inline bool operator!=(const Point3& other) const noexcept {
    return !(*this == other);
  }

  inline Point3& operator+=(const Vector3<T>& displacement) noexcept {
    m_xyz += displacement;
    return *this;
  }

  inline Point3& operator-=(const Vector3<T>& displacement) noexcept {
    m_xyz -= displacement;
    return *this;
  }

private:
  inline void checkInvariant() const {
    RT_GEOM_ASSERT(std::abs(m_w - T(1)) <= geom::pointWTolerance<T>);
  }

  Vector3<T> m_xyz;
  T m_w = T(1);
};

/** A unit-length displacement (ray, light and view directions). */
template<class T>
class Direction3 {
public:
  /** Asserts (when assertions are on) that v is unit length. */
  inline explicit Direction3(const Vector3<T>& unitVector)
      : m_v(unitVector) {
    RT_GEOM_ASSERT(geom::isUnitLength(m_v));
  }

  /** Normalizes the components. */
  inline static Direction3 fromComponents(const T& x, const T& y, const T& z) {
    return Direction3(Vector3<T>(x, y, z).normalized());
  }

  [[nodiscard]] inline T x() const noexcept { return m_v.x(); }
  [[nodiscard]] inline T y() const noexcept { return m_v.y(); }
  [[nodiscard]] inline T z() const noexcept { return m_v.z(); }

  /** Widening to a free-form vector is always safe, hence implicit. */
  [[nodiscard]] inline const Vector3<T>& toVector() const noexcept { return m_v; }
  inline operator const Vector3<T>&() const noexcept { return m_v; }

  [[nodiscard]] inline Direction3 operator-() const { return Direction3(-m_v); }

  [[nodiscard]] inline bool operator==(const Direction3& other) const noexcept {
    return m_v == other.m_v;
  }

  [[nodiscard]] inline bool operator!=(const Direction3& other) const noexcept {
    return !(*this == other);
  }

private:
  Vector3<T> m_v;
};

/** A unit-length surface normal. Transforms by the inverse-transpose. */
template<class T>
class Normal3 {
public:
  /** Asserts (when assertions are on) that v is unit length. */
  inline explicit Normal3(const Vector3<T>& unitVector)
      : m_v(unitVector) {
    RT_GEOM_ASSERT(geom::isUnitLength(m_v));
  }

  /** Normalizes the components. */
  inline static Normal3 fromComponents(const T& x, const T& y, const T& z) {
    return Normal3(Vector3<T>(x, y, z).normalized());
  }

  [[nodiscard]] inline T x() const noexcept { return m_v.x(); }
  [[nodiscard]] inline T y() const noexcept { return m_v.y(); }
  [[nodiscard]] inline T z() const noexcept { return m_v.z(); }

  [[nodiscard]] inline const Vector3<T>& toVector() const noexcept { return m_v; }
  inline operator const Vector3<T>&() const noexcept { return m_v; }

  [[nodiscard]] inline Normal3 operator-() const { return Normal3(-m_v); }

  [[nodiscard]] inline bool operator==(const Normal3& other) const noexcept {
    return m_v == other.m_v;
  }

  [[nodiscard]] inline bool operator!=(const Normal3& other) const noexcept {
    return !(*this == other);
  }

private:
  Vector3<T> m_v;
};

/** Normalizes v and tags it as a Direction3. */
template<class T>
[[nodiscard]] inline Direction3<T> toDirection(const Vector3<T>& v) {
  return Direction3<T>(v.normalized());
}

/** Normalizes v and tags it as a Normal3. */
template<class T>
[[nodiscard]] inline Normal3<T> toNormal(const Vector3<T>& v) {
  return Normal3<T>(v.normalized());
}

/** Reinterprets a Normal3 as a Direction3. Explicit: transform rules differ. */
template<class T>
[[nodiscard]] inline Direction3<T> toDirection(const Normal3<T>& n) {
  return Direction3<T>(n.toVector());
}

/** Reinterprets a Direction3 as a Normal3. Explicit: transform rules differ. */
template<class T>
[[nodiscard]] inline Normal3<T> toNormal(const Direction3<T>& d) {
  return Normal3<T>(d.toVector());
}

// ---------------------------------------------------------------------------
// Point arithmetic
// ---------------------------------------------------------------------------

template<class T>
[[nodiscard]] inline Point3<T> operator+(const Point3<T>& p, const Vector3<T>& v) {
  return Point3<T>(p.toVector() + v);
}

template<class T>
[[nodiscard]] inline Point3<T> operator+(const Vector3<T>& v, const Point3<T>& p) {
  return p + v;
}

template<class T>
[[nodiscard]] inline Point3<T> operator-(const Point3<T>& p, const Vector3<T>& v) {
  return Point3<T>(p.toVector() - v);
}

template<class T>
[[nodiscard]] inline Vector3<T> operator-(const Point3<T>& a, const Point3<T>& b) {
  return a.toVector() - b.toVector();
}

// Geometrically meaningless operations.
template<class T>
Point3<T> operator+(const Point3<T>&, const Point3<T>&) = delete;
template<class T, class S>
Point3<T> operator*(const Point3<T>&, const S&) = delete;
template<class T, class S>
Point3<T> operator*(const S&, const Point3<T>&) = delete;

// ---------------------------------------------------------------------------
// Direction / Normal arithmetic: results are no longer unit length -> Vector3
// ---------------------------------------------------------------------------

#define RT_GEOM_UNIT_ARITHMETIC(Unit)                                                      \
  template<class T>                                                                        \
  [[nodiscard]] inline Vector3<T> operator+(const Unit<T>& a, const Unit<T>& b) {          \
    return a.toVector() + b.toVector();                                                    \
  }                                                                                        \
  template<class T>                                                                        \
  [[nodiscard]] inline Vector3<T> operator-(const Unit<T>& a, const Unit<T>& b) {          \
    return a.toVector() - b.toVector();                                                    \
  }                                                                                        \
  template<class T>                                                                        \
  [[nodiscard]] inline Vector3<T> operator*(const Unit<T>& a, const T& s) {                \
    return a.toVector() * s;                                                               \
  }                                                                                        \
  template<class T>                                                                        \
  [[nodiscard]] inline Vector3<T> operator*(const T& s, const Unit<T>& a) {                \
    return a.toVector() * s;                                                               \
  }

RT_GEOM_UNIT_ARITHMETIC(Direction3)
RT_GEOM_UNIT_ARITHMETIC(Normal3)
#undef RT_GEOM_UNIT_ARITHMETIC

/** The cosine terms lighting integrands depend on. */
template<class T>
[[nodiscard]] inline T dot(const Direction3<T>& a, const Direction3<T>& b) noexcept {
  return a.toVector().dotProduct(b.toVector());
}

template<class T>
[[nodiscard]] inline T dot(const Normal3<T>& n, const Direction3<T>& d) noexcept {
  return n.toVector().dotProduct(d.toVector());
}

template<class T>
[[nodiscard]] inline T dot(const Direction3<T>& d, const Normal3<T>& n) noexcept {
  return dot(n, d);
}

// ---------------------------------------------------------------------------
// Matrix transforms
// ---------------------------------------------------------------------------

/**
  * The inverse-transpose of a matrix's linear part, for transforming normals.
  * Deliberately not convertible back to Matrix3; use asMatrix3() to opt out.
  */
template<class T>
class NormalMatrix3 {
public:
  /** Builds the inverse-transpose of m. Throws if m is singular. */
  inline explicit NormalMatrix3(const Matrix3<T>& linear)
      : m_inverseTranspose(linear.luDecomposition().inverse().transposed()) {
  }

  [[nodiscard]] inline Normal3<T> operator*(const Normal3<T>& n) const {
    return toNormal(m_inverseTranspose * n.toVector());
  }

  /** Composition: (M1 M2)^-T = M1^-T M2^-T. */
  [[nodiscard]] inline NormalMatrix3 operator*(const NormalMatrix3& other) const {
    return NormalMatrix3(m_inverseTranspose * other.m_inverseTranspose, Raw{});
  }

  /** Escape hatch to the raw inverse-transpose matrix. */
  [[nodiscard]] inline const Matrix3<T>& asMatrix3() const noexcept { return m_inverseTranspose; }

private:
  struct Raw {};
  inline NormalMatrix3(const Matrix3<T>& inverseTranspose, Raw)
      : m_inverseTranspose(inverseTranspose) {
  }

  Matrix3<T> m_inverseTranspose;
};

/** The upper-left 3x3 block of m. */
template<class T>
[[nodiscard]] inline Matrix3<T> linearPart(const Matrix4<T>& m) {
  return Matrix3<T>(m);
}

template<class T>
[[nodiscard]] inline NormalMatrix3<T> normalMatrix(const Matrix3<T>& m) {
  return NormalMatrix3<T>(m);
}

template<class T>
[[nodiscard]] inline NormalMatrix3<T> normalMatrix(const Matrix4<T>& m) {
  return NormalMatrix3<T>(linearPart(m));
}

/** Full affine transform: translation applies. */
template<class T>
[[nodiscard]] inline Point3<T> operator*(const Matrix4<T>& m, const Point3<T>& p) {
  return Point3<T>(m.transformPoint(p.toVector()));
}

/** Linear transform, renormalized because the matrix may scale. */
template<class T>
[[nodiscard]] inline Direction3<T> operator*(const Matrix3<T>& m, const Direction3<T>& d) {
  return toDirection(m * d.toVector());
}

/** Translation is ignored for directions, renormalized afterwards. */
template<class T>
[[nodiscard]] inline Direction3<T> operator*(const Matrix4<T>& m, const Direction3<T>& d) {
  return toDirection(m.transformDirection(d.toVector()));
}

// A forward matrix must never transform a normal: use normalMatrix().
template<class T>
Normal3<T> operator*(const Matrix3<T>&, const Normal3<T>&) = delete;
template<class T>
Normal3<T> operator*(const Matrix4<T>&, const Normal3<T>&) = delete;

typedef Point3<float> Point3f;
typedef Point3<double> Point3d;
typedef Direction3<float> Direction3f;
typedef Direction3<double> Direction3d;
typedef Normal3<float> Normal3f;
typedef Normal3<double> Normal3d;
typedef NormalMatrix3<float> NormalMatrix3f;
typedef NormalMatrix3<double> NormalMatrix3d;
