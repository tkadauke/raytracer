#pragma once

#include <cmath>
#include <iostream>
#include <limits>
#include "core/DivisionByZeroException.h"

/**
  * This is a generic vector class with a fixed number of dimensions Dimensions
  * and component type T. The last parameter VectorCellType is only relevant
  * for the actual storage of the data in memory. It defaults to the component
  * type.
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
  */
template<int Dimensions, class T, class VectorCellType = T>
class Vector {
public:
  typedef T CellsType[Dimensions];

private:
  static const int CellsTypeSize = sizeof(CellsType);
  static const int VectorCellTypeSize = sizeof(VectorCellType);
  static const int VectorCellCount = (CellsTypeSize + VectorCellTypeSize + 1) / VectorCellTypeSize;

  typedef VectorCellType VectorType[VectorCellCount];
  
public:
  typedef T Coordinate;
  static const int Dim = Dimensions;
  
  /**
    * Constructs a null vector \f$(0,\ldots,0)\f$.
    */
  inline Vector() {
    for (int i = 0; i != Dimensions; ++i) {
      m_coordinates[i] = T();
    }
  }

  /**
    * Constructs a vector component wise from the given array. The array's size
    * must be exactly the same as the dimensions of the vector.
    */
  inline Vector(const CellsType& cells) {
    for (int i = 0; i != Dimensions; ++i) {
      m_coordinates[i] = cells[i];
    }
  }

  /**
    * Constructs a Vector2<T> from an arbitrary-dimensioned and arbitrary-
    * typed source Vector. Any fields not contained in the source vector will
    * be initialized with zeroes.
    */
  template<int D, class C, class V>
  inline Vector(const Vector<D, C, V>& source) {
    for (int i = 0; i != Dimensions; ++i) {
      if (i >= D)
        m_coordinates[i] = T();
      else
        m_coordinates[i] = source.coordinate(i);
    }
  }

  /**
    * Returns the coordinate with index dim.
    */
  inline T coordinate(int dim) const {
    return m_coordinates[dim];
  }
  
  /**
    * Sets the coordinate with index dim to value.
    */
  inline void setCoordinate(int dim, const T& value) {
    m_coordinates[dim] = value;
  }
  
  /**
    * Array index operator for the vector. Returns a reference to the coordinate
    * with index dim, suitable for writing.
    */
  inline T& operator[](int dim) {
    return m_coordinates[dim];
  }
  
  /**
    * Constant array index operator for the vector. Returns a read-only
    * reference to the coordinate with index dim.
    */
  inline const T& operator[](int dim) const {
    return m_coordinates[dim];
  }
  
  /**
    * For this vector \f$v\f$ and other vector \f$u\f$, returns the sum of the
    * vectors \f$v+u = (v_1+u_1,\ldots,v_n+u_n)\f$.
    */
  inline Vector<Dimensions, T, VectorCellType> operator+(const Vector<Dimensions, T, VectorCellType>& other) const {
    Vector<Dimensions, T, VectorCellType> result;
    for (int i = 0; i != Dimensions; ++i) {
      result.setCoordinate(i, coordinate(i) + other.coordinate(i));
    }
    return result;
  }

  /**
    * For this vector \f$v\f$ and other vector \f$u\f$, returns the difference
    * of the vectors \f$v-u = (v_1-u_1,\ldots,v_n-u_n)\f$.
    */
  inline Vector<Dimensions, T, VectorCellType> operator-(const Vector<Dimensions, T, VectorCellType>& other) const {
    Vector<Dimensions, T, VectorCellType> result;
    for (int i = 0; i != Dimensions; ++i) {
      result.setCoordinate(i, coordinate(i) - other.coordinate(i));
    }
    return result;
  }

  /**
    * For this vector \f$v\f$, returns the negative of the vector
    * \f$-v = (-v_1,\ldots,-v_n)\f$.
    */
  inline Vector<Dimensions, T, VectorCellType> operator-() const {
    Vector<Dimensions, T, VectorCellType> result;
    for (int i = 0; i != Dimensions; ++i) {
      result.setCoordinate(i, - coordinate(i));
    }
    return result;
  }

  /**
    * For this vector \f$v\f$ and a constant factor \f$c\f$, returns the
    * quotient of the vector and the constant
    * \f$\frac{v}{c} = (v_1/c,\ldots,v_n/c)\f$. Throws a DivisionByZeroException
    * if factor is zero.
    */
  inline Vector<Dimensions, T, VectorCellType> operator/(const T& factor) const {
    if (factor == T())
      throw DivisionByZeroException(__FILE__, __LINE__);

    T recip = 1.0 / factor;
    return *this * recip;
  }
  
  /**
    * For this vector \f$v\f$ and other vector \f$u\f$, returns the dot product
    * of the vectors \f$v \cdot u = (v_1u_1,\ldots,v_nu_n)\f$.
    */
  inline T dotProduct(const Vector<Dimensions, T, VectorCellType>& other) const {
    T result = T();
    for (int i = 0; i != Dimensions; ++i) {
      result += coordinate(i) * other.coordinate(i);
    }
    return result;
  }

  /**
    * Synonym for dotProduct().
    */
  inline T operator*(const Vector<Dimensions, T, VectorCellType>& other) const {
    return dotProduct(other);
  }

  /**
    * For this vector \f$v\f$ and a constant factor \f$c\f$, returns the
    * product of the vector and the constant
    * \f$vc = (v_{1}c,\ldots,v_{n}c)\f$.
    */
  inline Vector<Dimensions, T, VectorCellType> operator*(const T& factor) const {
    Vector<Dimensions, T, VectorCellType> result;
    for (int i = 0; i != Dimensions; ++i) {
      result.setCoordinate(i, coordinate(i) * factor);
    }
    return result;
  }

  /**
    * Returns true if all components of this vector are equal to components of
    * the other vector.
    */
  inline bool operator==(const Vector<Dimensions, T, VectorCellType>& other) const {
    if (&other == this)
      return true;
    for (int i = 0; i != Dimensions; ++i) {
      if (coordinate(i) != other.coordinate(i))
        return false;
    }
    return true;
  }

  /**
    * Returns true if any of the components in this vector is different from the
    * corresponding component in the other vector.
    */
  inline bool operator!=(const Vector<Dimensions, T, VectorCellType>& other) const {
    return !(*this == other);
  }
  
  /**
    * Adds vector other to this vector, mutating this vector. See operator+().
    */
  inline Vector<Dimensions, T, VectorCellType>& operator+=(const Vector<Dimensions, T, VectorCellType>& other) {
    for (int i = 0; i != Dimensions; ++i) {
      setCoordinate(i, coordinate(i) + other.coordinate(i));
    }
    return *this;
  }

  /**
    * Subtracts other from this vector, mutating this vector. See operator-().
    */
  inline Vector<Dimensions, T, VectorCellType>& operator-=(const Vector<Dimensions, T, VectorCellType>& other) {
    for (int i = 0; i != Dimensions; ++i) {
      setCoordinate(i, coordinate(i) - other.coordinate(i));
    }
    return *this;
  }

  /**
    * Scales this vector by factor, mutating this vector. See operator*().
    */
  inline Vector<Dimensions, T, VectorCellType>& operator*=(const T& factor) {
    for (int i = 0; i != Dimensions; ++i) {
      setCoordinate(i, coordinate(i) * factor);
    }
    return *this;
  }

  /**
    * Divides this vector by factor, mutating this vector. Throws a
    * DivisionByZeroException if factor is zero. See operator/().
    */
  inline Vector<Dimensions, T, VectorCellType>& operator/=(const T& factor) {
    if (factor == T())
      throw DivisionByZeroException(__FILE__, __LINE__);

    T recip = 1.0 / factor;
    return this->operator*=(recip);
  }

  /**
    * Returns the length of vector \f$v\f$, i.e. \f$|v|\f$.
    */
  inline T length() const {
    return std::sqrt(squaredLength());
  }

  /**
    * Returns the square of the length of vector \f$v\f$, i.e. \f$v \cdot v\f$.
    */
  inline T squaredLength() const {
    return *this * *this;
  }
  
  /**
    * Interprets this vector \f$v\f$ as well as other \f$u\f$ as points and
    * returns the distance between those points, i.e. \f$|u-v|\f$.
    */
  inline T distanceTo(const Vector<Dimensions, T, VectorCellType>& other) {
    return (*this - other).length();
  }
  
  /**
    * Interprets this vector \f$v\f$ as well as other \f$u\f$ as points and
    * returns the squared distance between those points, i.e. \f$|u-v|^2\f$.
    */
  inline T squaredDistanceTo(const Vector<Dimensions, T, VectorCellType>& other) {
    return (*this - other).squaredLength();
  }

/**
  * For vector \f$v\f$, returns vector \f$\frac{v}{|v|}\f$, i.e. the vector
  * devided by its length, which is a unit vector with the same direction as the
  * original, but length 1.
  */
  inline Vector<Dimensions, T, VectorCellType> normalized() const {
    return *this / length();
  }
  
  /**
    * For vector \f$v\f$, turns this into vector \f$\frac{v}{|v|}\f$, i.e. the
    * vector devided by its length, which is a unit vector with the same
    * direction as the original, but length 1. This method mutates the object.
    */
  inline void normalize() {
    *this /= length();
  }

  /**
    * Returns true if the vector has length 1.
    */
  inline bool isNormalized() const {
    return length() == static_cast<T>(1);
  }
  
  /**
    * Returns true if any of the vector's components is NaN.
    */
  inline bool isUndefined() const {
    for (int i = 0; i != Dimensions; ++i) {
      if (std::isnan(coordinate(i)))
        return true;
    }
    return false;
  }
  
  /**
    * Returns true if and only if the vector is defined. See isUndefined().
    */
  inline bool isDefined() const {
    return !isUndefined();
  }

protected:
  union {
    CellsType m_coordinates;
    VectorType m_vector;
  };
};

/**
  * Generic print function for Vectors. Turns a vector \f$(1,2,3)\f$ to the
  * string \c "(1, 2, 3)".
  */
template<int Dimensions, class T, class VectorCellType>
std::ostream& operator<<(std::ostream& os, const Vector<Dimensions, T, VectorCellType>& vector) {
  os << "(";
  for (int i = 0; i != Dimensions; ++i) {
    os << vector[i];
    if (i < Dimensions - 1)
      os << ", ";
  }
  os << ")";
  return os;
}

template<int Dimensions, class T, class VectorType>
class SpecializedVector : public Vector<Dimensions, T> {
  typedef Vector<Dimensions, T> Base;
public:
  using Base::dotProduct;
  using Base::operator*;

  inline SpecializedVector()
    : Base()
  {
  }

  template<int D, class C, class V>
  inline SpecializedVector(const Vector<D, C, V>& source)
    : Base(source)
  {
  }
  
  inline VectorType operator+(const VectorType& other) const {
    return static_cast<VectorType>(this->Base::operator+(other));
  }

  inline VectorType operator-(const VectorType& other) const {
    return static_cast<VectorType>(this->Base::operator-(other));
  }

  inline VectorType operator-() const {
    return static_cast<VectorType>(this->Base::operator-());
  }

  inline VectorType operator/(const T& factor) const {
    return static_cast<VectorType>(this->Base::operator/(factor));
  }

  inline VectorType operator*(const T& factor) const {
    return static_cast<VectorType>(this->Base::operator*(factor));
  }
  
  inline VectorType& operator+=(const VectorType& other) {
    return static_cast<VectorType&>(this->Base::operator+=(other));
  }
  
  inline VectorType& operator-=(const VectorType& other) {
    return static_cast<VectorType&>(this->Base::operator-=(other));
  }

  inline VectorType& operator*=(const T& factor) {
    return static_cast<VectorType&>(this->Base::operator*=(factor));
  }

  inline VectorType& operator/=(const T& factor) {
    return static_cast<VectorType&>(this->Base::operator/=(factor));
  }

  inline VectorType normalized() const {
    return static_cast<VectorType>(this->Base::normalized());
  }
};

/**
  * Represents a two-dimensional vector with component type T. This class
  * implements all the important operations for vectors. Many of those are
  * defined in the parent classes. The operations defined in this class are
  * mostly specific to two-dimensional vectors.
  * 
  * Use this class to represent an absolute point on the plane, a directional
  * vector, or a normal.
  * 
  * This class defines a number of different constant vectors, such as the unit
  * vectors (right(), up()), the null vector null(), and the undefined() vector.
  * 
  * Construction of vectors is as expected. The default constructor creates the
  * null vector, there is a component-wise constructor, and a generic copy
  * constructor that converts any vector type to a three-dimensional vector.
  * 
  * As special operations only available to two-dimensional vectors, this
  * class implements the x(), and y() accessors.
  */
template<class T>
class Vector2 : public SpecializedVector<2, T, Vector2<T>> {
  typedef SpecializedVector<2, T, Vector2<T>> Base;
public:
  using Base::setCoordinate;
  
  /**
    * Returns the null vector \f$(0,0)\f$.
    */
  static const Vector2<T>& null() {
    static Vector2<T> v(0, 0);
    return v;
  }
  
  /**
    * Returns an undefined vector \f$(NaN,NaN)\f$.
    */
  static const Vector2<T>& undefined() {
    static Vector2<T> v(std::numeric_limits<T>::quiet_NaN(),
                        std::numeric_limits<T>::quiet_NaN());
    return v;
  }

  /**
    * Returns the right unit vector \f$(1,0)\f$.
    */
  static const Vector2<T>& right() {
    static Vector2<T> v(1, 0);
    return v;
  }

  /**
    * Returns the up unit vector \f$(0,1)\f$.
    */
  static const Vector2<T>& up() {
    static Vector2<T> v(0, 1);
    return v;
  }

  /**
    * Constructs a null vector \f$(0,0)\f$.
    */
  inline Vector2()
    : Base()
  {
  }

  /**
    * Constructs the vector \f$(x,y)\f$.
    */
  inline Vector2(const T& x, const T& y) {
    setCoordinate(0, x);
    setCoordinate(1, y);
  }

  /**
    * Constructs a Vector2<T> from an arbitrary-dimensioned and arbitrary-
    * typed source Vector.
    */
  template<int D, class C, class V>
  inline Vector2(const Vector<D, C, V>& source)
    : Base(source)
  {
  }

  /**
    * Returns the vector's first component, i.e. returns \f$x\f$ from
    * \f$(x,y)\f$.
    */
  inline T x() const {
    return Base::coordinate(0);
  }
  
  /**
    * Returns the vector's first component, i.e. returns \f$y\f$ from
    * \f$(x,y)\f$.
    */
  inline T y() const {
    return Base::coordinate(1);
  }
};

/**
  * Represents a three-dimensional vector with component type T. This class
  * implements all the important operations for vectors. Many of those are
  * defined in the parent classes. The operations defined in this class are
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
  */
template<class T>
class Vector3 : public SpecializedVector<3, T, Vector3<T>> {
  typedef SpecializedVector<3, T, Vector3<T>> Base;
public:
  using Base::setCoordinate;
  
  /**
    * Returns the null vector \f$(0,0,0)\f$.
    */
  static const Vector3<T>& null() {
    static Vector3<T> v(0, 0, 0);
    return v;
  }

  /**
    * Returns the null vector \f$(1,1,1)\f$.
    */
  static const Vector3<T>& one() {
    static Vector3<T> v(1, 1, 1);
    return v;
  }
  
  /**
    * Returns \f$(\epsilon,\epsilon,\epsilon)\f$.
    */
  static const Vector3<T>& epsilon() {
    static Vector3<T> v(std::numeric_limits<T>::epsilon(),
                        std::numeric_limits<T>::epsilon(),
                        std::numeric_limits<T>::epsilon());
    return v;
  }
  
  /**
    * Returns an undefined vector: \f$(NaN,NaN,NaN)\f$.
    */
  static const Vector3<T>& undefined() {
    static Vector3<T> v(std::numeric_limits<T>::quiet_NaN(),
                        std::numeric_limits<T>::quiet_NaN(),
                        std::numeric_limits<T>::quiet_NaN());
    return v;
  }
  
  /**
    * Returns \f$(-\infty,-\infty,-\infty)\f$.
    */
  static const Vector3<T>& minusInfinity() {
    static Vector3<T> v(-std::numeric_limits<T>::infinity(),
                        -std::numeric_limits<T>::infinity(),
                        -std::numeric_limits<T>::infinity());
    return v;
  }
  
  /**
    * Returns \f$(\infty,\infty,\infty)\f$.
    */
  static const Vector3<T>& plusInfinity() {
    static Vector3<T> v(std::numeric_limits<T>::infinity(),
                        std::numeric_limits<T>::infinity(),
                        std::numeric_limits<T>::infinity());
    return v;
  }

  /**
    * Returns the right unit vector \f$(1,0,0)\f$.
    */
  static const Vector3<T>& right() {
    static Vector3<T> v(1, 0, 0);
    return v;
  }

  /**
    * Returns the up unit vector \f$(0,1,0)\f$.
    */
  static const Vector3<T>& up() {
    static Vector3<T> v(0, 1, 0);
    return v;
  }

  /**
    * Returns the forward unit vector \f$(0,0,1)\f$.
    */
  static const Vector3<T>& forward() {
    static Vector3<T> v(0, 0, 1);
    return v;
  }

  /**
    * Default constructor. Returns the null vector \f$(0,0,0)\f$.
    */
  inline Vector3()
    : Base()
  {
  }

  /**
    * Constructs the vector \f$(x,y,z)\f$. The \f$y\f$ parameter is optional
    * and defaults to 0, which is handy for vectors in the two-dimensional
    * plane.
    */
  inline Vector3(const T& x, const T& y, const T& z = 0) {
    setCoordinate(0, x);
    setCoordinate(1, y);
    setCoordinate(2, z);
  }

  /**
    * Constructs a Vector3<T> from an arbitrary-dimensioned and arbitrary-
    * typed source Vector.
    */
  template<int D, class C, class V>
  inline Vector3(const Vector<D, C, V>& source)
    : Base(source)
  {
  }

  /**
    * Returns the vector's first component, i.e. returns \f$x\f$ from
    * \f$(x,y,z)\f$.
    */
  inline T x() const {
    return Base::coordinate(0);
  }

  /**
    * Returns the vector's second component, i.e. returns \f$y\f$ from
    * \f$(x,y,z)\f$.
    */
  inline T y() const {
    return Base::coordinate(1);
  }

  /**
    * Returns the vector's third component, i.e. returns \f$z\f$ from
    * \f$(x,y,z)\f$.
    */
  inline T z() const {
    return Base::coordinate(2);
  }
  
  /**
    * For this vector \f$u\f$ and the other vector \f$v\f$, returns the cross
    * product \f$u \times v\f$.
    */
  inline Vector3<T> crossProduct(const Vector3<T>& other) const {
    return Vector3<T>(y() * other.z() - z() * other.y(),
                      z() * other.x() - x() * other.z(),
                      x() * other.y() - y() * other.x());
  }

  /**
    * Synonym for crossProduct().
    */
  inline Vector3<T> operator^(const Vector3<T>& other) const {
    return crossProduct(other);
  }
};

/**
  * Represents a four-dimensional vector with component type T. This class
  * implements all the important operations for vectors. Many of those are
  * defined in the parent classes. The operations defined in this class are
  * mostly specific to three-dimensional vectors.
  * 
  * Use this class to represent for absolute-point transformations. By default,
  * all four-dimensional vectors have \f$1\f$ as the fourth \f$w\f$ component.
  * This is in order to be able to translate points in three-dimensional space.
  * 
  * This class defines only a few constants, namely the null() vector, as well
  * as the undefined() vector. The null() vector still has \f$1\f$ as the
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
  * on the machine.
  */
template<class T>
class Vector4 : public SpecializedVector<4, T, Vector4<T>> {
  typedef SpecializedVector<4, T, Vector4<T>> Base;
public:
  using Base::setCoordinate;
  
  /**
    * Returns the null vector \f$(0,0,0,1)\f$, which notably contains a \f$1\f$
    * for the \f$w\f$ component.
    */
  static const Vector4<T>& null() {
    static Vector4<T> v(0, 0, 0, 1);
    return v;
  }
  
  /**
    * Returns the epsilon vector \f$(\epsilon,\epsilon,\epsilon,\epsilon)\f$,
    * which is minimally shifted from the origin in all directions.
    */
  static const Vector4<T>& epsilon() {
    static Vector4<T> v(std::numeric_limits<T>::epsilon(),
                        std::numeric_limits<T>::epsilon(),
                        std::numeric_limits<T>::epsilon(),
                        std::numeric_limits<T>::epsilon());
    return v;
  }
  
  /**
    * Returns an undefined vector \f$(NaN,NaN,NaN,NaN)\f$.
    */
  static const Vector4<T>& undefined() {
    static Vector4<T> v(std::numeric_limits<T>::quiet_NaN(),
                        std::numeric_limits<T>::quiet_NaN(),
                        std::numeric_limits<T>::quiet_NaN(),
                        std::numeric_limits<T>::quiet_NaN());
    return v;
  }

  /**
    * Returns the vector \f$(-\infty,-\infty,-\infty,-\infty)\f$.
    */
  static const Vector4<T>& minusInfinity() {
    static Vector4<T> v(-std::numeric_limits<T>::infinity(),
                        -std::numeric_limits<T>::infinity(),
                        -std::numeric_limits<T>::infinity(),
                        -std::numeric_limits<T>::infinity());
    return v;
  }

  /**
    * Returns the vector \f$(\infty,\infty,\infty,\infty)\f$.
    */
  static const Vector4<T>& plusInfinity() {
    static Vector4<T> v(std::numeric_limits<T>::infinity(),
                        std::numeric_limits<T>::infinity(),
                        std::numeric_limits<T>::infinity(),
                        std::numeric_limits<T>::infinity());
    return v;
  }

  /**
    * Constructs the null vector, but sets the \f$w\f$ component to \f$1\f$.
    */
  inline Vector4()
    : Base()
  {
    setCoordinate(3, T(1));
  }

  /**
    * Component-wise constructor that sets the vector to \f$(x,y,z,w)\f$, where
    * \f$w\f$ defaults to \f$1\f$ for convenience.
    */
  inline Vector4(const T& x, const T& y, const T& z, const T& w = 1) {
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
  template<int D, class C, class V>
  inline Vector4(const Vector<D, C, V>& source)
    : Base(source)
  {
    if (D != 4)
      setCoordinate(3, T(1));
  }

  /**
    * Returns the vector's first component, i.e. returns \f$x\f$ from
    * \f$(x,y,z,w)\f$.
    */
  inline T x() const {
    return Base::coordinate(0);
  }
  
  /**
    * Returns the vector's second component, i.e. returns \f$y\f$ from
    * \f$(x,y,z,w)\f$.
    */
  inline T y() const {
    return Base::coordinate(1);
  }
  
  /**
    * Returns the vector's third component, i.e. returns \f$z\f$ from
    * \f$(x,y,z,w)\f$.
    */
  inline T z() const {
    return Base::coordinate(2);
  }
  
  /**
    * Returns the vector's fourth component, i.e. returns \f$w\f$ from
    * \f$(x,y,z,w)\f$.
    */
  inline T w() const {
    return Base::coordinate(3);
  }
  
  /**
    * Returns the vector's homogenized three-dimensional vector, i.e. returns
    * \f$\frac{(x,y,z)}{w}\f$.
    */
  inline Vector3<T> homogenized() const {
    return Vector3<T>(*this) / w();
  }
};

#include "core/math/vector/sse3/Vector3f.h"
#include "core/math/vector/sse3/Vector4f.h"
#include "core/math/vector/sse3/Vector3d.h"
#include "core/math/vector/sse3/Vector4d.h"

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
