#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <tuple>
#include <type_traits>
#include <algorithm>
#include "core/DivisionByZeroException.h"

/**
  * This is a generic vector class with a fixed number of dimensions Dimensions
  * and component type T. The third parameter StorageCellType is only relevant
  * for the actual storage of the data in memory. It defaults to the component
  * type. The last parameter Derived should be set to the deriving class type,
  * however it is possible to omit this parameter and use this class directly.
  * 
  * This vector type implements many of the operations that are defined for
  * vectors of arbitrary dimension, like operator+(), operator*(), or length().
  * It features a default null-vector constructor, a copy constructor for C
  * arrays and a generic copy constructor for any Vector type of arbitrary
  * dimension and coordinate type. Additionally, many of the operations exist in
  * two different versions, one that changes the original vector (like
  * operator+=()), and one that returns a new vector (like operator+()).
  * 
  * For the most common dimensions (2, 3, 4) and coordinate type (float, double)
  * there are specialized subclasses of this class, so you won't have to use
  * this class directly. These are called Vector2, Vector3, and Vector4,
  * respectively, and each have a type parameter for the coordinate type. For
  * even more convenience, there are typedefs for float and double coordinate
  * types in combination with each of the specialized classes, resulting in
  * six different types: Vector2f, Vector2d, Vector3f, Vector3d, Vector4f, and
  * Vector4d. There are SSE-optimized template specializations for some of
  * these predefined Vector types to improve performance.
  * 
  * @tparam Dimensions the number of dimensions for this vector type.
  * @tparam T the coordinate type, usually a floating point type.
  * @tparam StorageCellType the type used internally to store the vector
  *   components. This defaults to T, but for the SSE-optimized vectors, wider
  *   types are used for storage.
  * @tparam Derived the derived class, if any. Defaults to void. If
  *   this is void, then all the calculation operations, like +, -,
  *   etc., accept as argument and return an object of type Vector. If Derived
  *   is set explicitely, then the operators accept and return objects of type
  *   Derived. This way, operators don't have to be redefined in the subclasses.
  *   See Vector::VectorType for details.
  */
template<int Dimensions, class T, class StorageCellType = T, class Derived = void>
class Vector {
public:
  typedef T CellsType[Dimensions];

private:
  static const int CellsTypeSize = sizeof(CellsType);
  static const int StorageCellTypeSize = sizeof(StorageCellType);
  static const int StorageCellCount = (Dimensions * sizeof(T) - 1) / StorageCellTypeSize + 1;

  typedef StorageCellType StorageType[StorageCellCount];
  
  typedef Vector<Dimensions, T, StorageCellType, Derived> ThisType;
    
public:
  /**
    * Corrdinate type. Usually a floating point type like float or double.
    */
  typedef T Coordinate;
  
  /**
    * Number of dimensions of this vector type.
    */
  static const int Dim = Dimensions;
  
  /**
    * Type for arguments and return types of many operators defined in this
    * class. If the Derived template parameter is omitted, then this is
    * equivalent to Vector. Otherwise, it is equivalent to Derived.
    */
  using VectorType = std::conditional_t<
    std::is_same_v<Derived, void>,
    ThisType,
    Derived
  >;

  /**
    * Constructs a null vector \f$(0,\ldots,0)\f$.
    */
  inline constexpr Vector() : m_coordinates{} {
  }

  /**
    * Constructs a vector component wise from the given array. The array's size
    * must be exactly the same as the dimensions of the vector.
    */
  inline constexpr explicit Vector(const CellsType& cells) : m_coordinates{} {
    for (int i = 0; i != Dimensions; ++i) {
      m_coordinates[i] = cells[i];
    }
  }

  /**
    * Component-wise constructor: accepts exactly Dimensions values convertible
    * to T.  Allows Vector<3, float, float, void>(x, y, z) for the generic
    * base type (used in SIMD regression tests and vertex-dedup benchmarks).
    * The concrete subclasses (Vector3<T> etc.) provide their own typed
    * constructors that take priority when called on the derived type.
    */
  template<class... Args,
           typename = std::enable_if_t<
             sizeof...(Args) == static_cast<std::size_t>(Dimensions) &&
             (std::is_convertible_v<Args, T> && ...)>>
  inline constexpr explicit Vector(Args&&... args) : m_coordinates{} {
    T values[] = {static_cast<T>(std::forward<Args>(args))...};
    for (int i = 0; i != Dimensions; ++i)
      m_coordinates[i] = values[i];
  }

  /**
    * Constructs a Vector<T> from an arbitrary-dimensioned and arbitrary-
    * typed source Vector. Any fields not contained in the source vector will
    * be initialized with zeroes.
    */
  template<int D, class C, class S, class V>
  inline constexpr Vector(const Vector<D, C, S, V>& source) : m_coordinates{} {
    for (int i = 0; i != Dimensions; ++i) {
      if (i >= D)
        m_coordinates[i] = T();
      else
        m_coordinates[i] = source.coordinate(i);
    }
  }

  /**
    * @returns the coordinate with index dim.
    */
  [[nodiscard]] inline constexpr T coordinate(int dim) const noexcept {
    return m_coordinates[dim];
  }

  /**
    * Sets the coordinate with index dim to value.
    */
  inline constexpr void setCoordinate(int dim, const T& value) noexcept {
    m_coordinates[dim] = value;
  }

  /**
    * Array index operator for the vector.
    *
    * @returns a reference to the coordinate with index dim, suitable for
    *   writing.
    */
  inline constexpr T& operator[](int dim) noexcept {
    return m_coordinates[dim];
  }

  /**
    * Constant array index operator for the vector.
    *
    * @returns a read-only reference to the coordinate with index dim.
    */
  [[nodiscard]] inline constexpr const T& operator[](int dim) const noexcept {
    return m_coordinates[dim];
  }
  
  /**
    * @returns the sum of the vectors \f$v+u = (v_1+u_1,\ldots,v_n+u_n)\f$, for
    *   where this vector is \f$v\f$ and other is \f$u\f$.
    */
  [[nodiscard]] inline constexpr VectorType operator+(const VectorType& other) const noexcept {
    VectorType result;
    for (int i = 0; i != Dimensions; ++i) {
      result.setCoordinate(i, coordinate(i) + other.coordinate(i));
    }
    return result;
  }

  /**
    * @returns the difference of the vectors \f$v-u = (v_1-u_1,\ldots,v_n-u_n)\f$,
    *   where this vector is \f$v\f$ and other is \f$u\f$.
    */
  [[nodiscard]] inline constexpr VectorType operator-(const VectorType& other) const noexcept {
    VectorType result;
    for (int i = 0; i != Dimensions; ++i) {
      result.setCoordinate(i, coordinate(i) - other.coordinate(i));
    }
    return result;
  }

  /**
    * @returns the negative of the vector \f$-v = (-v_1,\ldots,-v_n)\f$, where
    *   this vector is \f$v\f$.
    */
  [[nodiscard]] inline constexpr VectorType operator-() const noexcept {
    VectorType result;
    for (int i = 0; i != Dimensions; ++i) {
      result.setCoordinate(i, - coordinate(i));
    }
    return result;
  }

  /**
    * @returns the quotient of the vector and the constant \f$\frac{v}{c} =
    *   (v_1/c,\ldots,v_n/c)\f$, where this vector is \f$v\f$ and factor is
    *   \f$c\f$.
    * @throws a DivisionByZeroException if factor is zero.
    */
  [[nodiscard]] inline VectorType operator/(const T& factor) const {
    if (factor == T())
      throw DivisionByZeroException(__FILE__, __LINE__);

    T recip = 1.0 / factor;
    return derived() * recip;
  }

  /**
    * @returns the dot product of the vectors \f$v \cdot u = (v_1u_1,\ldots,
    *   v_nu_n)\f$, where this vector is \f$v\f$ and other is \f$u\f$.
    */
  [[nodiscard]] inline constexpr T dotProduct(const VectorType& other) const noexcept {
    T result = T();
    for (int i = 0; i != Dimensions; ++i) {
      result += coordinate(i) * other.coordinate(i);
    }
    return result;
  }

  /**
    * Synonym for dotProduct().
    */
  [[nodiscard]] inline constexpr T operator*(const VectorType& other) const noexcept {
    return dotProduct(other);
  }

  /**
    * @returns the product of the vector and the constant \f$vc = (v_{1}c,\ldots,
    *   v_{n}c)\f$, where this vector is \f$v\f$ and factor is \f$c\f$
    */
  [[nodiscard]] inline constexpr VectorType operator*(const T& factor) const noexcept {
    VectorType result;
    for (int i = 0; i != Dimensions; ++i) {
      result.setCoordinate(i, coordinate(i) * factor);
    }
    return result;
  }

  /**
    * Returns true if any of the components in this vector is different from the
    * corresponding component in the other vector.
    */
  [[nodiscard]] inline constexpr bool operator!=(const VectorType& other) const noexcept {
    return !(derived() == other);
  }

  /**
    * @returns true if all components of this vector are equal to components of
    *   the other vector, false otherwise.
    */
  [[nodiscard]] inline constexpr bool operator==(const VectorType& other) const noexcept {
    if (&other == &derived())
      return true;
    for (int i = 0; i != Dimensions; ++i) {
      if (coordinate(i) != other.coordinate(i))
        return false;
    }
    return true;
  }

  /**
    * Adds vector other to this vector, mutating this vector. See operator+().
    *
    * @returns itself.
    */
  inline constexpr VectorType& operator+=(const VectorType& other) noexcept {
    for (int i = 0; i != Dimensions; ++i) {
      setCoordinate(i, coordinate(i) + other.coordinate(i));
    }
    return derived();
  }

  /**
    * Subtracts other from this vector, mutating this vector. See operator-().
    *
    * @returns itself.
    */
  inline constexpr VectorType& operator-=(const VectorType& other) noexcept {
    for (int i = 0; i != Dimensions; ++i) {
      setCoordinate(i, coordinate(i) - other.coordinate(i));
    }
    return derived();
  }

  /**
    * Scales this vector by factor, mutating this vector. See operator*().
    *
    * @returns itself.
    */
  inline constexpr VectorType& operator*=(const T& factor) noexcept {
    for (int i = 0; i != Dimensions; ++i) {
      setCoordinate(i, coordinate(i) * factor);
    }
    return derived();
  }

  /**
    * Divides this vector by factor, mutating this vector. Throws a
    * DivisionByZeroException if factor is zero. See operator/().
    *
    * @returns itself.
    */
  inline VectorType& operator/=(const T& factor) {
    if (factor == T())
      throw DivisionByZeroException(__FILE__, __LINE__);

    T recip = 1.0 / factor;
    return derived().operator*=(recip);
  }

  /**
    * @returns the length of this vector \f$v\f$, i.e. \f$|v|\f$.
    */
  [[nodiscard]] inline T length() const noexcept {
    return std::sqrt(squaredLength());
  }

  /**
    * @returns the square of the length of this vector \f$v\f$, i.e. \f$v \cdot
    *   v\f$.
    */
  [[nodiscard]] inline constexpr T squaredLength() const noexcept {
    return derived() * derived();
  }

  /**
    * Interprets this vector \f$v\f$ as well as other \f$u\f$ as points.
    *
    * @returns the distance between those points, i.e. \f$|u-v|\f$.
    */
  [[nodiscard]] inline T distanceTo(const VectorType& other) const noexcept {
    return (derived() - other).length();
  }

  /**
    * Interprets this vector \f$v\f$ as well as other \f$u\f$ as points.
    *
    * @returns the squared distance between those points, i.e. \f$|u-v|^2\f$.
    */
  [[nodiscard]] inline constexpr T squaredDistanceTo(const VectorType& other) const noexcept {
    return (derived() - other).squaredLength();
  }

  /**
    * @returns the reversed vector to this vector. For vector \f$v\f$, this is
    *   equivalent to writing \f$-v\f$.
    */
  [[nodiscard]] inline constexpr VectorType reversed() const noexcept {
    return -derived();
  }

  /**
    * Reverses this vector in place, i.e. negates all its components.
    */
  inline constexpr void reverse() noexcept {
    derived() = -derived();
  }

  /**
    * @returns vector \f$\frac{v}{|v|}\f$, where this vector is \f$v\f$, i.e.
    * this vector devided by its length, which is a unit vector with the same
    * direction as the original, but length 1.
    */
  [[nodiscard]] inline VectorType normalized() const {
    return derived() / length();
  }

  /**
    * For vector \f$v\f$, turns this into vector \f$\frac{v}{|v|}\f$, i.e. the
    * vector devided by its length, which is a unit vector with the same
    * direction as the original, but length 1. This method mutates the object.
    */
  inline void normalize() {
    derived() /= length();
  }

  /**
    * @returns true if the vector has length 1, false otherwise.
    */
  [[nodiscard]] inline bool isNormalized() const noexcept {
    return length() == T(1);
  }

  /**
    * @returns true if any of the vector's components is NaN, false otherwise.
    */
  [[nodiscard]] inline bool isUndefined() const noexcept {
    for (int i = 0; i != Dimensions; ++i) {
      if (std::isnan(coordinate(i)))
        return true;
    }
    return false;
  }

  /**
    * @returns true if any of the vector's components is +inf or -inf, false
    * otherwise.
    */
  [[nodiscard]] inline bool isInfinite() const noexcept {
    for (int i = 0; i != Dimensions; ++i) {
      if (std::isinf(coordinate(i)))
        return true;
    }
    return false;
  }

  /**
    * @returns true if the vector is defined, false otherwise. Opposite of
    * isUndefined().
    */
  [[nodiscard]] inline bool isDefined() const noexcept {
    return !isUndefined();
  }

  /**
    * @returns true if the vector is the null vector, i.e. all its components
    *   are 0, false otherwise.
    */
  [[nodiscard]] inline constexpr bool isNull() const noexcept {
    for (int i = 0; i != Dimensions; ++i) {
      if (coordinate(i) != T())
        return false;
    }
    return true;
  }

  /**
    * @returns the minimum component of this vector.
    */
  [[nodiscard]] inline constexpr T min() const noexcept {
    T result = coordinate(0);
    for (int i = 1; i != Dimensions; ++i)
      result = std::min(result, coordinate(i));
    return result;
  }

  /**
    * @returns the maximum component of this vector.
    */
  [[nodiscard]] inline constexpr T max() const noexcept {
    T result = coordinate(0);
    for (int i = 1; i != Dimensions; ++i)
      result = std::max(result, coordinate(i));
    return result;
  }

  /**
    * @returns a vector with an absolute value for all components.
    */
  [[nodiscard]] inline VectorType abs() const noexcept {
    VectorType result;
    for (int i = 0; i != Dimensions; ++i)
      result.setCoordinate(i, std::abs(coordinate(i)));
    return result;
  }

  /**
    * @returns the component-wise minimum of this vector and other,
    *   i.e. \f$(\min(v_1,u_1),\ldots,\min(v_n,u_n))\f$.
    */
  [[nodiscard]] inline constexpr VectorType cwiseMin(const VectorType& other) const noexcept {
    VectorType result;
    for (int i = 0; i != Dimensions; ++i)
      result.setCoordinate(i, std::min(coordinate(i), other.coordinate(i)));
    return result;
  }

  /**
    * @returns the component-wise maximum of this vector and other,
    *   i.e. \f$(\max(v_1,u_1),\ldots,\max(v_n,u_n))\f$.
    */
  [[nodiscard]] inline constexpr VectorType cwiseMax(const VectorType& other) const noexcept {
    VectorType result;
    for (int i = 0; i != Dimensions; ++i)
      result.setCoordinate(i, std::max(coordinate(i), other.coordinate(i)));
    return result;
  }

  /**
    * @returns this vector with each component clamped to \f$[lo, hi]\f$.
    */
  [[nodiscard]] inline constexpr VectorType clamp(const T& lo, const T& hi) const noexcept {
    VectorType result;
    for (int i = 0; i != Dimensions; ++i)
      result.setCoordinate(i, std::max(lo, std::min(hi, coordinate(i))));
    return result;
  }

  /**
    * @returns this vector with each component clamped to \f$[0, 1]\f$.
    */
  [[nodiscard]] inline constexpr VectorType saturate() const noexcept {
    return clamp(T(0), T(1));
  }

  /**
    * @returns the linear interpolation between this vector and other at
    *   parameter \f$t\f$, i.e. \f$v + t(u - v) = (1-t)v + tu\f$.
    */
  [[nodiscard]] inline constexpr VectorType lerp(const VectorType& other, const T& t) const noexcept {
    return derived() + (other - derived()) * t;
  }

  /**
    * @returns the reflection of this vector around unit normal \f$n\f$,
    *   i.e. \f$v - 2(v \cdot n)n\f$.
    *
    * Both this vector and \f$n\f$ are expected to point away from the
    * surface (same side). The returned direction also points away from
    * the surface.
    */
  [[nodiscard]] inline constexpr VectorType reflect(const VectorType& n) const noexcept {
    return derived() - n * (T(2) * (derived() * n));
  }

  /**
    * @returns the refracted direction of this vector through a surface
    *   with unit outward normal \f$n\f$ and refractive-index ratio
    *   \f$\eta = n_\text{inside}/n_\text{outside}\f$.
    *
    * Preconditions: (1) this vector and \f$n\f$ are on the same side
    * (dot product is positive), (2) no total internal reflection —
    * the caller should test \f$1-(1-\cos^2\theta)/\eta^2 \geq 0\f$
    * before calling.
    */
  [[nodiscard]] inline VectorType refract(const VectorType& n, const T& eta) const {
    T cosTheta = derived() * n;
    T cosTheta2 = std::sqrt(T(1) - (T(1) - cosTheta * cosTheta) / (eta * eta));
    return -(derived() / eta) - n * (cosTheta2 - cosTheta / eta);
  }

  /**
    * @returns true if this vector is approximately equal to other within
    *   component-wise absolute tolerance \f$\epsilon\f$.
    */
  [[nodiscard]] inline bool approxEqual(const VectorType& other, const T& epsilon) const noexcept {
    for (int i = 0; i != Dimensions; ++i) {
      if (std::abs(coordinate(i) - other.coordinate(i)) > epsilon)
        return false;
    }
    return true;
  }

protected:
  [[nodiscard]] inline constexpr const VectorType& derived() const noexcept {
    return static_cast<const VectorType&>(*this);
  }

  [[nodiscard]] inline constexpr VectorType& derived() noexcept {
    return static_cast<VectorType&>(*this);
  }

  union {
    CellsType m_coordinates;
    StorageType m_vector;
  };
};

/**
  * Generic string serialization function for Vectors. Turns a vector
  * \f$(1,2,3)\f$ into string \c "(1, 2, 3)" and streams it to os.
  * 
  * @returns os.
  */
template<int Dimensions, class T, class StorageCellType, class Derived>
std::ostream& operator<<(std::ostream& os, const Vector<Dimensions, T, StorageCellType, Derived>& vector) {
  os << "(";
  for (int i = 0; i != Dimensions; ++i) {
    os << vector[i];
    if (i < Dimensions - 1)
      os << ", ";
  }
  os << ")";
  return os;
}

/**
  * @returns the product of the vector and the constant \f$vc = (v_{1}c,\ldots,
  *   v_{n}c)\f$, where this vector is \f$v\f$ and factor is \f$c\f$
  */
template<int Dimensions, class T, class StorageCellType, class Derived>
[[nodiscard]] inline constexpr typename Vector<Dimensions, T, StorageCellType, Derived>::VectorType
operator*(const T& factor, const Vector<Dimensions, T, StorageCellType, Derived>& vector) noexcept {
  return vector * factor;
}

/**
  * Represents a two-dimensional vector with component type T. This class
  * implements all the important operations for vectors. Many of those are
  * defined in the parent class. The operations defined in this class are
  * mostly specific to two-dimensional vectors.
  * 
  * Use this class to represent an absolute point on the plane, a directional
  * vector, or a normal.
  * 
  * This class defines a number of different constant vectors, such as the unit
  * vectors (right(), up()), the null vector null, and the undefined vector.
  * 
  * Construction of vectors is as expected. The default constructor creates the
  * null vector, there is a component-wise constructor, and a generic copy
  * constructor that converts any vector type to a two-dimensional vector.
  * 
  * As special operations only available to two-dimensional vectors, this
  * class implements the x(), and y() accessors.
  *
  * @tparam T the coordinate type, usually a floating point type.
  */
template<class T>
class Vector2 : public Vector<2, T, T, Vector2<T>> {
  typedef Vector<2, T, T, Vector2<T>> Base;
public:
  using Base::setCoordinate;
  
  /**
    * The null vector \f$(0,0)\f$.
    */
  static const Vector2<T> null;

  /**
    * An undefined vector \f$(NaN,NaN)\f$.
    */
  static const Vector2<T> undefined;

  /**
    * @returns the right unit vector \f$(1,0)\f$.
    */
  static const Vector2<T>& right() {
    static Vector2<T> v(1, 0);
    return v;
  }

  /**
    * @returns the up unit vector \f$(0,1)\f$.
    */
  static const Vector2<T>& up() {
    static Vector2<T> v(0, 1);
    return v;
  }

  /**
    * Constructs a null vector \f$(0,0)\f$.
    */
  inline constexpr Vector2()
    : Base()
  {
  }

  /**
    * Constructs the vector \f$(x,y)\f$.
    */
  inline constexpr Vector2(const T& x, const T& y) {
    setCoordinate(0, x);
    setCoordinate(1, y);
  }

  /**
    * Constructs a Vector2<T> from an arbitrary-dimensioned and arbitrary-
    * typed source Vector.
    */
  template<int D, class C, class S, class V>
  inline constexpr Vector2(const Vector<D, C, S, V>& source)
    : Base(source)
  {
  }

  /**
    * @returns the vector's first component, i.e. returns \f$x\f$ from
    *   \f$(x,y)\f$.
    */
  [[nodiscard]] inline constexpr T x() const noexcept {
    return Base::coordinate(0);
  }

  /**
    * Sets the vector's first component to @p value.
    */
  inline constexpr void setX(const T& value) noexcept {
    Base::setCoordinate(0, value);
  }

  /**
    * @returns the vector's second component, i.e. returns \f$y\f$ from
    *   \f$(x,y)\f$.
    */
  [[nodiscard]] inline constexpr T y() const noexcept {
    return Base::coordinate(1);
  }

  /**
    * Sets the vector's second component to @p value.
    */
  inline constexpr void setY(const T& value) noexcept {
    Base::setCoordinate(1, value);
  }
};

/**
  * Represents a three-dimensional vector with component type T. This class
  * implements all the important operations for vectors. Many of those are
  * defined in the parent class. The operations defined in this class are
  * mostly specific to three-dimensional vectors.
  * 
  * Use this class to represent an absolute point in space, a directional
  * vector, or a normal.
  * 
  * This class defines a number of different constant vectors, such as all the
  * unit vectors (right(), up(), forward()), the null vector null(), two
  * infinity vectors (minusInfinity(), plusInfinity()), among others.
  * 
  * Construction of vectors is as expected. The default constructor creates the
  * null vector, there is a component-wise constructor, and a generic copy
  * constructor that converts any vector type to a three-dimensional vector.
  * 
  * As special operations only available to three-dimensional vectors, this
  * class implements the x(), y(), and z() accessors, as well as the
  * crossProduct() function, or its operator^() synonym.
  *
  * There are two predefined types for three-dimensional vectors: Vector3f with
  * float-typed components and Vector3d with double-typed components. Both of
  * those have SSE-optimized template specializations, in case SSE is available
  * on the machine.
  *
  * @tparam T the coordinate type, usually a floating point type.
  */
template<class T>
class Vector3 : public Vector<3, T, T, Vector3<T>> {
  typedef Vector<3, T, T, Vector3<T>> Base;
public:
  using Base::setCoordinate;
  
  /**
    * The null vector \f$(0,0,0)\f$.
    */
  static const Vector3<T> null;

  /**
    * The one vector \f$(1,1,1)\f$.
    */
  static const Vector3<T> one;

  /**
    * \f$(\epsilon,\epsilon,\epsilon)\f$.
    */
  static const Vector3<T> epsilon;

  /**
    * An undefined vector: \f$(NaN,NaN,NaN)\f$.
    */
  static const Vector3<T> undefined;

  /**
    * \f$(-\infty,-\infty,-\infty)\f$.
    */
  static const Vector3<T> minusInfinity;

  /**
    * \f$(\infty,\infty,\infty)\f$.
    */
  static const Vector3<T> plusInfinity;

  /**
    * @returns the right unit vector \f$(1,0,0)\f$.
    */
  static const Vector3<T>& right() {
    static Vector3<T> v(1, 0, 0);
    return v;
  }

  /**
    * @returns the up unit vector \f$(0,1,0)\f$.
    */
  static const Vector3<T>& up() {
    static Vector3<T> v(0, 1, 0);
    return v;
  }

  /**
    * @returns the forward unit vector \f$(0,0,1)\f$.
    */
  static const Vector3<T>& forward() {
    static Vector3<T> v(0, 0, 1);
    return v;
  }

  /**
    * Default constructor. Constructs the null vector \f$(0,0,0)\f$.
    */
  inline constexpr Vector3()
    : Base()
  {
  }

  /**
    * Constructs the vector \f$(x,y,z)\f$. The \f$y\f$ parameter is optional
    * and defaults to 0, which is handy for vectors in the two-dimensional
    * plane.
    */
  inline constexpr Vector3(const T& x, const T& y, const T& z = 0) {
    setCoordinate(0, x);
    setCoordinate(1, y);
    setCoordinate(2, z);
  }

  /**
    * Constructs a Vector3<T> from an arbitrary-dimensioned and arbitrary-
    * typed source Vector.
    */
  template<int D, class C, class S, class V>
  inline constexpr Vector3(const Vector<D, C, S, V>& source)
    : Base(source)
  {
  }

  /**
    * @returns this vector's first component, i.e. returns \f$x\f$ from
    *   \f$(x,y,z)\f$.
    */
  [[nodiscard]] inline constexpr T x() const noexcept {
    return Base::coordinate(0);
  }

  /**
    * Sets the vector's first component to @p value.
    */
  inline constexpr void setX(const T& value) noexcept {
    Base::setCoordinate(0, value);
  }

  /**
    * @returns this vector's second component, i.e. returns \f$y\f$ from
    *   \f$(x,y,z)\f$.
    */
  [[nodiscard]] inline constexpr T y() const noexcept {
    return Base::coordinate(1);
  }

  /**
    * Sets the vector's second component to @p value.
    */
  inline constexpr void setY(const T& value) noexcept {
    Base::setCoordinate(1, value);
  }

  /**
    * @returns this vector's third component, i.e. returns \f$z\f$ from
    *   \f$(x,y,z)\f$.
    */
  [[nodiscard]] inline constexpr T z() const noexcept {
    return Base::coordinate(2);
  }

  /**
    * Sets the vector's third component to @p value.
    */
  inline constexpr void setZ(const T& value) noexcept {
    Base::setCoordinate(2, value);
  }

  /**
    * @returns the cross product \f$u \times v\f$, where this vector is \f$u\f$
    *   and other is \f$v\f$.
    */
  [[nodiscard]] inline constexpr Vector3<T> crossProduct(const Vector3<T>& other) const noexcept {
    return Vector3<T>(y() * other.z() - z() * other.y(),
                      z() * other.x() - x() * other.z(),
                      x() * other.y() - y() * other.x());
  }

  /**
    * Synonym for crossProduct().
    */
  [[nodiscard]] inline constexpr Vector3<T> operator^(const Vector3<T>& other) const noexcept {
    return crossProduct(other);
  }
};

/**
  * Represents a four-dimensional vector with component type T. This class
  * implements all the important operations for vectors. Many of those are
  * defined in the parent class. The operations defined in this class are
  * mostly specific to four-dimensional vectors.
  * 
  * Use this class to calculate absolute-point transformations. By default, all
  * four-dimensional vectors have \f$1\f$ as the fourth \f$w\f$ component. This
  * is in order to be able to translate points in three-dimensional space.
  * 
  * This class defines only a few constants, namely the null vector, as well
  * as the undefined vector. The null vector still has \f$1\f$ as the
  * \f$w\f$ component. For any other three-dimensional constant, create a
  * Vector3 and convert it to this class.
  * 
  * \code
  * Vector4<double> vec = Vector4<double>(Vector3<double>::up());
  * \endcode
  * 
  * Construction of vectors is as expected. The default constructor creates the
  * null vector, there is a component-wise constructor, and a generic copy
  * constructor that converts any vector type to a three-dimensional vector.
  * This copy constructor makes sure the \f$w\f$ component is \f$1\f$, if the
  * source vector is three-dimensional or smaller.
  * 
  * As special operations only available to four-dimensional vectors, this
  * class implements the x(), y(), z(), and w() accessors, as well as the
  * homogenized() function, which divides the vector by its w() component.
  *
  * There are two predefined types for four-dimensional vectors: Vector4f with
  * float-typed components and Vector4d with double-typed components. Both of
  * those have SSE-optimized template specializations, in case SSE is available
  * at compile time.
  *
  * @tparam T the coordinate type, usually a floating point type.
  */
template<class T>
class Vector4 : public Vector<4, T, T, Vector4<T>> {
  typedef Vector<4, T, T, Vector4<T>> Base;
public:
  using Base::setCoordinate;
  
  /**
    * The null vector \f$(0,0,0,1)\f$, which notably contains a \f$1\f$
    *   for the \f$w\f$ component.
    */
  static const Vector4<T> null;

  /**
    * The epsilon vector \f$(\epsilon,\epsilon,\epsilon,\epsilon)\f$,
    *   which is minimally shifted from the origin in all directions.
    */
  static const Vector4<T> epsilon;

  /**
    * An undefined vector \f$(NaN,NaN,NaN,NaN)\f$.
    */
  static const Vector4<T> undefined;

  /**
    * The vector \f$(-\infty,-\infty,-\infty,-\infty)\f$.
    */
  static const Vector4<T> minusInfinity;

  /**
    * The vector \f$(\infty,\infty,\infty,\infty)\f$.
    */
  static const Vector4<T> plusInfinity;

  /**
    * Constructs the null vector, but sets the \f$w\f$ component to \f$1\f$.
    */
  inline constexpr Vector4()
    : Base()
  {
    setCoordinate(3, T(1));
  }

  /**
    * Component-wise constructor that sets the vector to \f$(x,y,z,w)\f$, where
    *   \f$w\f$ defaults to \f$1\f$ for convenience.
    */
  inline constexpr Vector4(const T& x, const T& y, const T& z, const T& w = 1) {
    setCoordinate(0, x);
    setCoordinate(1, y);
    setCoordinate(2, z);
    setCoordinate(3, w);
  }

  /**
    * Constructs a Vector4<T> from an arbitrary-dimensioned and arbitrary-
    * typed source Vector. If the source vector is not four dimensional, the
    * constructor sets the \f$w\f$ component to \f$1\f$.
    */
  template<int D, class C, class S, class V>
  inline constexpr Vector4(const Vector<D, C, S, V>& source)
    : Base(source)
  {
    if (D != 4)
      setCoordinate(3, T(1));
  }

  /**
    * @returns this vector's first component, i.e. returns \f$x\f$ from
    *   \f$(x,y,z,w)\f$.
    */
  [[nodiscard]] inline constexpr T x() const noexcept {
    return Base::coordinate(0);
  }

  /**
    * Sets the vector's first component to @p value.
    */
  inline constexpr void setX(const T& value) noexcept {
    Base::setCoordinate(0, value);
  }

  /**
    * @returns this vector's second component, i.e. returns \f$y\f$ from
    *   \f$(x,y,z,w)\f$.
    */
  [[nodiscard]] inline constexpr T y() const noexcept {
    return Base::coordinate(1);
  }

  /**
    * Sets the vector's second component to @p value.
    */
  inline constexpr void setY(const T& value) noexcept {
    Base::setCoordinate(1, value);
  }

  /**
    * @returns this vector's third component, i.e. returns \f$z\f$ from
    *   \f$(x,y,z,w)\f$.
    */
  [[nodiscard]] inline constexpr T z() const noexcept {
    return Base::coordinate(2);
  }

  /**
    * Sets the vector's third component to @p value.
    */
  inline constexpr void setZ(const T& value) noexcept {
    Base::setCoordinate(2, value);
  }

  /**
    * @returns this vector's fourth component, i.e. returns \f$w\f$ from
    *   \f$(x,y,z,w)\f$.
    */
  [[nodiscard]] inline constexpr T w() const noexcept {
    return Base::coordinate(3);
  }

  /**
    * Sets the vector's fourth component to @p value.
    */
  inline constexpr void setW(const T& value) noexcept {
    Base::setCoordinate(3, value);
  }

  /**
    * @returns ths vector's homogenized three-dimensional vector, i.e. returns
    *   \f$\frac{(x,y,z)}{w}\f$.
    */
  [[nodiscard]] inline Vector3<T> homogenized() const {
    return Vector3<T>(*this) / w();
  }
};

// ---------------------------------------------------------------------------
// Out-of-class definitions for inline constexpr constants (C++17).
// The declarations are in the class bodies above; definitions are here so that
// the class type is complete at the point of initialization.
// SSE3 specializations below override these for float and double.
// ---------------------------------------------------------------------------

template<class T>
inline constexpr Vector2<T> Vector2<T>::null{T(0), T(0)};

template<class T>
inline constexpr Vector2<T> Vector2<T>::undefined{
  std::numeric_limits<T>::quiet_NaN(),
  std::numeric_limits<T>::quiet_NaN()
};

template<class T>
inline constexpr Vector3<T> Vector3<T>::null{T(0), T(0), T(0)};

template<class T>
inline constexpr Vector3<T> Vector3<T>::one{T(1), T(1), T(1)};

template<class T>
inline constexpr Vector3<T> Vector3<T>::epsilon{
  std::numeric_limits<T>::epsilon(),
  std::numeric_limits<T>::epsilon(),
  std::numeric_limits<T>::epsilon()
};

template<class T>
inline constexpr Vector3<T> Vector3<T>::undefined{
  std::numeric_limits<T>::quiet_NaN(),
  std::numeric_limits<T>::quiet_NaN(),
  std::numeric_limits<T>::quiet_NaN()
};

template<class T>
inline constexpr Vector3<T> Vector3<T>::minusInfinity{
  -std::numeric_limits<T>::infinity(),
  -std::numeric_limits<T>::infinity(),
  -std::numeric_limits<T>::infinity()
};

template<class T>
inline constexpr Vector3<T> Vector3<T>::plusInfinity{
  std::numeric_limits<T>::infinity(),
  std::numeric_limits<T>::infinity(),
  std::numeric_limits<T>::infinity()
};

template<class T>
inline constexpr Vector4<T> Vector4<T>::null{T(0), T(0), T(0), T(1)};

template<class T>
inline constexpr Vector4<T> Vector4<T>::epsilon{
  std::numeric_limits<T>::epsilon(),
  std::numeric_limits<T>::epsilon(),
  std::numeric_limits<T>::epsilon(),
  std::numeric_limits<T>::epsilon()
};

template<class T>
inline constexpr Vector4<T> Vector4<T>::undefined{
  std::numeric_limits<T>::quiet_NaN(),
  std::numeric_limits<T>::quiet_NaN(),
  std::numeric_limits<T>::quiet_NaN(),
  std::numeric_limits<T>::quiet_NaN()
};

template<class T>
inline constexpr Vector4<T> Vector4<T>::minusInfinity{
  -std::numeric_limits<T>::infinity(),
  -std::numeric_limits<T>::infinity(),
  -std::numeric_limits<T>::infinity(),
  -std::numeric_limits<T>::infinity()
};

template<class T>
inline constexpr Vector4<T> Vector4<T>::plusInfinity{
  std::numeric_limits<T>::infinity(),
  std::numeric_limits<T>::infinity(),
  std::numeric_limits<T>::infinity(),
  std::numeric_limits<T>::infinity()
};

// __m128/__m128d carry an alignment attribute that is silently dropped when
// used as a template argument.  The drop is harmless (the union's storage
// alignment is already handled by __m128 itself), but GCC/Clang emit
// -Wignored-attributes for it.  Suppress it around these four headers only.
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif
#include "core/math/vector/sse3/Vector3f.h"
#include "core/math/vector/sse3/Vector4f.h"
#include "core/math/vector/sse3/Vector3d.h"
#include "core/math/vector/sse3/Vector4d.h"

// The pop is deferred to after the typedef aliases and structured-bindings
// support below, since those also reference the SSE3-specialised types and
// would otherwise trigger -Wignored-attributes outside the suppressed block.

/**
  * Two-dimensional vector with float components.
  */
typedef Vector2<float> Vector2f;

/**
  * Two-dimensional vector with double components.
  */
typedef Vector2<double> Vector2d;

/**
  * Three-dimensional vector with float components.
  */
typedef Vector3<float> Vector3f;

/**
  * Three-dimensional vector with double components.
  */
typedef Vector3<double> Vector3d;

/**
  * Four-dimensional vector with float components.
  */
typedef Vector4<float> Vector4f;

/**
  * Four-dimensional vector with double components.
  */
typedef Vector4<double> Vector4d;

// ---------------------------------------------------------------------------
// Structured-bindings support (C++17).
//
// Enables: auto [x, y, z] = someVector3d;
//          auto [x, y]    = someVector2f;
//          auto [x, y, z, w] = someVector4f;
//
// All concrete Vector types store components accessible via coordinate(i),
// so a single set of free get<> overloads covers both the generic and the
// SSE3-specialized paths.
// ---------------------------------------------------------------------------

namespace std {  // NOLINT(cert-dcl58-cpp) — extending std for UDTs is allowed
  template<class T>
  struct tuple_size<Vector2<T>> : integral_constant<size_t, 2> {};

  template<size_t I, class T>
  struct tuple_element<I, Vector2<T>> { using type = T; };

  template<class T>
  struct tuple_size<Vector3<T>> : integral_constant<size_t, 3> {};

  template<size_t I, class T>
  struct tuple_element<I, Vector3<T>> { using type = T; };

  template<class T>
  struct tuple_size<Vector4<T>> : integral_constant<size_t, 4> {};

  template<size_t I, class T>
  struct tuple_element<I, Vector4<T>> { using type = T; };
}

template<size_t I, class T>
[[nodiscard]] inline constexpr T get(const Vector2<T>& v) noexcept { static_assert(I < 2u, "Vector2 index out of range"); return v.coordinate(static_cast<int>(I)); }

template<size_t I, class T>
[[nodiscard]] inline constexpr T get(const Vector3<T>& v) noexcept { static_assert(I < 3u, "Vector3 index out of range"); return v.coordinate(static_cast<int>(I)); }

template<size_t I, class T>
[[nodiscard]] inline constexpr T get(const Vector4<T>& v) noexcept { static_assert(I < 4u, "Vector4 index out of range"); return v.coordinate(static_cast<int>(I)); }

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

// ---------------------------------------------------------------------------
// std::hash specializations — enables unordered_map/unordered_set keys.
// ---------------------------------------------------------------------------
namespace std {  // NOLINT(cert-dcl58-cpp) — extending std for UDTs is allowed

  template<int Dimensions, class T, class StorageCellType, class Derived>
  struct hash<Vector<Dimensions, T, StorageCellType, Derived>> {
    size_t operator()(const Vector<Dimensions, T, StorageCellType, Derived>& v) const noexcept {
      size_t seed = 0;
      hash<T> h;
      for (int i = 0; i < Dimensions; ++i)
        seed ^= h(v.coordinate(i)) + size_t(0x9e3779b9) + (seed << 6) + (seed >> 2);
      return seed;
    }
  };

  template<class T>
  struct hash<Vector2<T>> {
    size_t operator()(const Vector2<T>& v) const noexcept {
      size_t seed = hash<T>{}(v.coordinate(0));
      seed ^= hash<T>{}(v.coordinate(1)) + size_t(0x9e3779b9) + (seed << 6) + (seed >> 2);
      return seed;
    }
  };

  template<class T>
  struct hash<Vector3<T>> {
    size_t operator()(const Vector3<T>& v) const noexcept {
      size_t seed = hash<T>{}(v.coordinate(0));
      seed ^= hash<T>{}(v.coordinate(1)) + size_t(0x9e3779b9) + (seed << 6) + (seed >> 2);
      seed ^= hash<T>{}(v.coordinate(2)) + size_t(0x9e3779b9) + (seed << 6) + (seed >> 2);
      return seed;
    }
  };

  template<class T>
  struct hash<Vector4<T>> {
    size_t operator()(const Vector4<T>& v) const noexcept {
      size_t seed = hash<T>{}(v.coordinate(0));
      seed ^= hash<T>{}(v.coordinate(1)) + size_t(0x9e3779b9) + (seed << 6) + (seed >> 2);
      seed ^= hash<T>{}(v.coordinate(2)) + size_t(0x9e3779b9) + (seed << 6) + (seed >> 2);
      seed ^= hash<T>{}(v.coordinate(3)) + size_t(0x9e3779b9) + (seed << 6) + (seed >> 2);
      return seed;
    }
  };
}

// ---------------------------------------------------------------------------
// std::formatter specializations (C++20). Fallback: the operator<< above.
// ---------------------------------------------------------------------------
#if defined(__cplusplus) && __cplusplus >= 202002L
#  if __has_include(<format>)
#    include <format>
#  endif
#endif

#ifdef __cpp_lib_format

template<int Dimensions, class T, class StorageCellType, class Derived>
struct std::formatter<Vector<Dimensions, T, StorageCellType, Derived>> {  // NOLINT(cert-dcl58-cpp)
  constexpr auto parse(std::format_parse_context& ctx) const { return ctx.begin(); }
  auto format(const Vector<Dimensions, T, StorageCellType, Derived>& v, std::format_context& ctx) const {
    auto out = std::format_to(ctx.out(), "(");
    for (int i = 0; i < Dimensions; ++i) {
      if (i > 0) out = std::format_to(out, ", ");
      out = std::format_to(out, "{}", v.coordinate(i));
    }
    return std::format_to(out, ")");
  }
};

template<class T>
struct std::formatter<Vector2<T>> {  // NOLINT(cert-dcl58-cpp)
  constexpr auto parse(std::format_parse_context& ctx) const { return ctx.begin(); }
  auto format(const Vector2<T>& v, std::format_context& ctx) const {
    return std::format_to(ctx.out(), "({}, {})", v.coordinate(0), v.coordinate(1));
  }
};

template<class T>
struct std::formatter<Vector3<T>> {  // NOLINT(cert-dcl58-cpp)
  constexpr auto parse(std::format_parse_context& ctx) const { return ctx.begin(); }
  auto format(const Vector3<T>& v, std::format_context& ctx) const {
    return std::format_to(ctx.out(), "({}, {}, {})", v.coordinate(0), v.coordinate(1), v.coordinate(2));
  }
};

template<class T>
struct std::formatter<Vector4<T>> {  // NOLINT(cert-dcl58-cpp)
  constexpr auto parse(std::format_parse_context& ctx) const { return ctx.begin(); }
  auto format(const Vector4<T>& v, std::format_context& ctx) const {
    return std::format_to(ctx.out(), "({}, {}, {}, {})", v.coordinate(0), v.coordinate(1), v.coordinate(2), v.coordinate(3));
  }
};

#endif  // __cpp_lib_format
