#ifndef VECTOR_H
#define VECTOR_H

#include <cmath>
#include <iostream>
#include <limits>
#include "core/DivisionByZeroException.h"

template<int Dimensions, class T, class VectorCellType = T>
class Vector {
  typedef T CellsType[Dimensions];

  static const int CellsTypeSize = sizeof(CellsType);
  static const int VectorCellTypeSize = sizeof(VectorCellType);
  static const int VectorCellCount = (CellsTypeSize + VectorCellTypeSize + 1) / VectorCellTypeSize;

  typedef VectorCellType VectorType[VectorCellCount];
  
public:
  typedef T Coordinate;
  static const int Dim = Dimensions;
  
  inline Vector() {
    for (int i = 0; i != Dimensions; ++i) {
      m_coordinates[i] = T();
    }
  }

  inline Vector(const CellsType& cells) {
    for (int i = 0; i != Dimensions; ++i) {
      m_coordinates[i] = cells[i];
    }
  }

  template<int D, class C, class V>
  inline Vector(const Vector<D, C, V>& source) {
    for (int i = 0; i != Dimensions; ++i) {
      if (i >= D)
        m_coordinates[i] = T();
      else
        m_coordinates[i] = source.coordinate(i);
    }
  }

  inline T coordinate(int dim) const { return m_coordinates[dim]; }
  inline void setCoordinate(int dim, const T& value) { m_coordinates[dim] = value; }

  inline T& operator[](int dim) { return m_coordinates[dim]; }
  inline const T& operator[](int dim) const { return m_coordinates[dim]; }

  inline Vector<Dimensions, T, VectorCellType> operator+(const Vector<Dimensions, T, VectorCellType>& other) const {
    Vector<Dimensions, T, VectorCellType> result;
    for (int i = 0; i != Dimensions; ++i) {
      result.setCoordinate(i, coordinate(i) + other.coordinate(i));
    }
    return result;
  }

  inline Vector<Dimensions, T, VectorCellType> operator-(const Vector<Dimensions, T, VectorCellType>& other) const {
    Vector<Dimensions, T, VectorCellType> result;
    for (int i = 0; i != Dimensions; ++i) {
      result.setCoordinate(i, coordinate(i) - other.coordinate(i));
    }
    return result;
  }

  inline Vector<Dimensions, T, VectorCellType> operator-() const {
    Vector<Dimensions, T, VectorCellType> result;
    for (int i = 0; i != Dimensions; ++i) {
      result.setCoordinate(i, - coordinate(i));
    }
    return result;
  }

  inline Vector<Dimensions, T, VectorCellType> operator/(const T& factor) const {
    if (factor == T())
      throw DivisionByZeroException(__FILE__, __LINE__);

    T recip = 1.0 / factor;
    return *this * recip;
  }
  
  inline T operator*(const Vector<Dimensions, T, VectorCellType>& other) const {
    T result = T();
    for (int i = 0; i != Dimensions; ++i) {
      result += coordinate(i) * other.coordinate(i);
    }
    return result;
  }

  inline Vector<Dimensions, T, VectorCellType> operator*(const T& factor) const {
    Vector<Dimensions, T, VectorCellType> result;
    for (int i = 0; i != Dimensions; ++i) {
      result.setCoordinate(i, coordinate(i) * factor);
    }
    return result;
  }
  
  inline bool operator==(const Vector<Dimensions, T, VectorCellType>& other) const {
    if (&other == this)
      return true;
    for (int i = 0; i != Dimensions; ++i) {
      if (coordinate(i) != other.coordinate(i))
        return false;
    }
    return true;
  }

  inline bool operator!=(const Vector<Dimensions, T, VectorCellType>& other) const {
    return !(*this == other);
  }
  
  inline Vector<Dimensions, T, VectorCellType>& operator+=(const Vector<Dimensions, T, VectorCellType>& other) {
    for (int i = 0; i != Dimensions; ++i) {
      setCoordinate(i, coordinate(i) + other.coordinate(i));
    }
    return *this;
  }

  inline Vector<Dimensions, T, VectorCellType>& operator-=(const Vector<Dimensions, T, VectorCellType>& other) {
    for (int i = 0; i != Dimensions; ++i) {
      setCoordinate(i, coordinate(i) - other.coordinate(i));
    }
    return *this;
  }

  inline Vector<Dimensions, T, VectorCellType>& operator*=(const T& factor) {
    for (int i = 0; i != Dimensions; ++i) {
      setCoordinate(i, coordinate(i) * factor);
    }
    return *this;
  }

  inline Vector<Dimensions, T, VectorCellType>& operator/=(const T& factor) {
    if (factor == T())
      throw DivisionByZeroException(__FILE__, __LINE__);

    T recip = 1.0 / factor;
    return this->operator*=(recip);
  }

  inline T length() const {
    return std::sqrt(squaredLength());
  }

  inline T squaredLength() const {
    return *this * *this;
  }
  
  inline T distanceTo(const Vector<Dimensions, T, VectorCellType>& other) {
    return (*this - other).length();
  }
  
  inline T squaredDistanceTo(const Vector<Dimensions, T, VectorCellType>& other) {
    return (*this - other).squaredLength();
  }

  inline Vector<Dimensions, T, VectorCellType> normalized() const {
    return *this / length();
  }
  
  inline void normalize() {
    *this /= length();
  }

  inline bool isNormalized() const {
    return length() == static_cast<T>(1);
  }
  
  inline bool isUndefined() const {
    for (int i = 0; i != Dimensions; ++i) {
      if (std::isnan(coordinate(i)))
        return true;
    }
    return false;
  }
  
  inline bool isDefined() const {
    return !isUndefined();
  }

protected:
  union {
    CellsType m_coordinates;
    VectorType m_vector;
  };
};

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

  inline T operator*(const Vector<Dimensions, T>& other) const {
    return this->Base::operator*(other);
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

template<class T>
class Vector2 : public SpecializedVector<2, T, Vector2<T> > {
  typedef SpecializedVector<2, T, Vector2<T> > Base;
public:
  using Base::setCoordinate;
  
  static const Vector2<T>& null() {
    static Vector2<T>* v = new Vector2<T>();
    return *v;
  }
  
  static const Vector2<T>& undefined() {
    static Vector2<T>* v = new Vector2<T>(std::numeric_limits<T>::quiet_NaN(),
                                          std::numeric_limits<T>::quiet_NaN());
    return *v;
  }

  inline Vector2()
    : Base()
  {
  }

  inline Vector2(const T& x, const T& y) {
    setCoordinate(0, x);
    setCoordinate(1, y);
  }

  template<int D, class C, class V>
  inline Vector2(const Vector<D, C, V>& source)
    : Base(source)
  {
  }

  inline T x() const { return Base::coordinate(0); }
  inline T y() const { return Base::coordinate(1); }
};

template<class T>
class Vector3 : public SpecializedVector<3, T, Vector3<T> > {
  typedef SpecializedVector<3, T, Vector3<T> > Base;
public:
  static const Vector3<T>& null() {
    static Vector3<T>* v = new Vector3<T>();
    return *v;
  }
  
  static const Vector3<T>& epsilon() {
    static Vector3<T>* v = new Vector3<T>(std::numeric_limits<T>::epsilon(),
                                          std::numeric_limits<T>::epsilon(),
                                          std::numeric_limits<T>::epsilon());
    return *v;
  }
  
  static const Vector3<T>& undefined() {
    static Vector3<T>* v = new Vector3<T>(std::numeric_limits<T>::quiet_NaN(),
                                          std::numeric_limits<T>::quiet_NaN(),
                                          std::numeric_limits<T>::quiet_NaN());
    return *v;
  }
  
  static const Vector3<T>& minusInfinity() {
    static Vector3<T>* v = new Vector3<T>(-std::numeric_limits<T>::infinity(),
                                          -std::numeric_limits<T>::infinity(),
                                          -std::numeric_limits<T>::infinity());
    return *v;
  }
  
  static const Vector3<T>& plusInfinity() {
    static Vector3<T>* v = new Vector3<T>(std::numeric_limits<T>::infinity(),
                                          std::numeric_limits<T>::infinity(),
                                          std::numeric_limits<T>::infinity());
    return *v;
  }

  inline Vector3()
    : Base()
  {
  }

  inline Vector3(const T& x, const T& y, const T& z = 0) {
    setCoordinate(0, x);
    setCoordinate(1, y);
    setCoordinate(2, z);
  }

  template<int D, class C, class V>
  inline Vector3(const Vector<D, C, V>& source)
    : Base(source)
  {
  }

  inline T x() const { return Base::coordinate(0); }
  inline T y() const { return Base::coordinate(1); }
  inline T z() const { return Base::coordinate(2); }
  
  inline Vector3<T> operator^(const Vector3<T>& other) const {
    return Vector3<T>(y() * other.z() - z() * other.y(),
                      z() * other.x() - x() * other.z(),
                      x() * other.y() - y() * other.x());
  }
};

template<class T>
class Vector4 : public SpecializedVector<4, T, Vector4<T> > {
  typedef SpecializedVector<4, T, Vector4<T> > Base;
public:
  static const Vector4<T>& null() {
    static Vector4<T>* v = new Vector4<T>();
    return *v;
  }
  
  static const Vector4<T>& undefined() {
    static Vector4<T>* v = new Vector4<T>(std::numeric_limits<T>::quiet_NaN(),
                                          std::numeric_limits<T>::quiet_NaN(),
                                          std::numeric_limits<T>::quiet_NaN(),
                                          std::numeric_limits<T>::quiet_NaN());
    return *v;
  }

  inline Vector4()
    : Base()
  {
    setCoordinate(3, T(1));
  }

  inline Vector4(const T& x, const T& y, const T& z, const T& w = 1) {
    setCoordinate(0, x);
    setCoordinate(1, y);
    setCoordinate(2, z);
    setCoordinate(3, w);
  }

  template<int D, class C, class V>
  inline Vector4(const Vector<D, C, V>& source)
    : Base(source)
  {
    if (D != 4)
      setCoordinate(3, T(1));
  }

  inline T x() const { return Base::coordinate(0); }
  inline T y() const { return Base::coordinate(1); }
  inline T z() const { return Base::coordinate(2); }
  inline T w() const { return Base::coordinate(3); }
  
  inline Vector3<T> homogenized() const {
    return Vector3<T>(*this) / w();
  }
};

#include "core/math/vector/sse3/Vector3f.h"
#include "core/math/vector/sse3/Vector4f.h"
#include "core/math/vector/sse3/Vector3d.h"
#include "core/math/vector/sse3/Vector4d.h"

typedef Vector2<float> Vector2f;
typedef Vector2<double> Vector2d;
typedef Vector3<float> Vector3f;
typedef Vector3<double> Vector3d;
typedef Vector4<float> Vector4f;
typedef Vector4<double> Vector4d;

#endif
