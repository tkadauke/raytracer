#ifndef VECTOR_H
#define VECTOR_H

#include <cmath>
#include <iostream>
#include "DivisionByZeroException.h"

template<int Dimensions, class T>
class Vector {
  typedef T CellsType[Dimensions];
public:
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

  template<int D>
  inline Vector(const Vector<D, T>& source) {
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

  inline Vector<Dimensions, T> operator+(const Vector<Dimensions, T>& other) const {
    Vector<Dimensions, T> result;
    for (int i = 0; i != Dimensions; ++i) {
      result.setCoordinate(i, coordinate(i) + other.coordinate(i));
    }
    return result;
  }

  inline Vector<Dimensions, T> operator-(const Vector<Dimensions, T>& other) const {
    Vector<Dimensions, T> result;
    for (int i = 0; i != Dimensions; ++i) {
      result.setCoordinate(i, coordinate(i) - other.coordinate(i));
    }
    return result;
  }

  inline Vector<Dimensions, T> operator-() const {
    Vector<Dimensions, T> result;
    for (int i = 0; i != Dimensions; ++i) {
      result.setCoordinate(i, - coordinate(i));
    }
    return result;
  }

  inline Vector<Dimensions, T> operator/(const T& factor) const {
    if (factor == T())
      throw DivisionByZeroException(__FILE__, __LINE__);

    Vector<Dimensions, T> result;
    for (int i = 0; i != Dimensions; ++i) {
      result.setCoordinate(i, coordinate(i) / factor);
    }
    return result;
  }
  
  inline T operator*(const Vector<Dimensions, T>& other) {
    T result = T();
    for (int i = 0; i != Dimensions; ++i) {
      result += coordinate(i) * other.coordinate(i);
    }
    return result;
  }

  inline Vector<Dimensions, T> operator*(const T& factor) const {
    Vector<Dimensions, T> result;
    for (int i = 0; i != Dimensions; ++i) {
      result.setCoordinate(i, coordinate(i) * factor);
    }
    return result;
  }

  inline bool operator==(const Vector<Dimensions, T>& other) const {
    for (int i = 0; i != Dimensions; ++i) {
      if (coordinate(i) != other.coordinate(i))
        return false;
    }
    return true;
  }

  inline bool operator!=(const Vector<Dimensions, T>& other) const {
    return !(*this == other);
  }

  inline T length() const {
    T result = 0;
    for (int i = 0; i != Dimensions; ++i) {
      result += coordinate(i) * coordinate(i);
    }
    return std::sqrt(result);
  }

  inline Vector<Dimensions, T> normalized() const {
    return *this / length();
  }

  inline bool isNormalized() const {
    return length() == static_cast<T>(1);
  }

private:
  T m_coordinates[Dimensions];
};

template<int Dimensions, class T>
std::ostream& operator<<(std::ostream& os, const Vector<Dimensions, T>& vector) {
  for (int i = 0; i != Dimensions; ++i) {
    os << vector[i] << ' ';
  }
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

  template<int D>
  inline SpecializedVector(const Vector<D, T>& source)
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

  inline VectorType normalized() const {
    return static_cast<VectorType>(this->Base::normalized());
  }
};

template<class T>
class Vector2 : public SpecializedVector<2, T, Vector2<T> > {
  typedef SpecializedVector<2, T, Vector2<T> > Base;
public:
  static const Vector2<T> null;

  inline Vector2()
    : Base()
  {
  }

  inline Vector2(const T& x, const T& y) {
    setCoordinate(0, x);
    setCoordinate(1, y);
  }

  template<int D>
  inline Vector2(const Vector<D, T>& source)
    : Base(source)
  {
  }

  inline T x() const { return Base::coordinate(0); }
  inline T y() const { return Base::coordinate(1); }
};

template<class T>
const Vector2<T> Vector2<T>::null = Vector2<T>();

typedef Vector2<float> Vector2f;
typedef Vector2<double> Vector2d;

template<class T>
class Vector3 : public SpecializedVector<3, T, Vector3<T> > {
  typedef SpecializedVector<3, T, Vector3<T> > Base;
public:
  static const Vector3<T> null;

  inline Vector3()
    : Base()
  {
  }

  inline Vector3(const T& x, const T& y, const T& z = 0) {
    setCoordinate(0, x);
    setCoordinate(1, y);
    setCoordinate(2, z);
  }

  template<int D>
  inline Vector3(const Vector<D, T>& source)
    : Base(source)
  {
  }

  inline T x() const { return Base::coordinate(0); }
  inline T y() const { return Base::coordinate(1); }
  inline T z() const { return Base::coordinate(2); }
};

template<class T>
const Vector3<T> Vector3<T>::null = Vector3<T>();

typedef Vector3<float> Vector3f;
typedef Vector3<double> Vector3d;

template<class T>
class Vector4 : public SpecializedVector<4, T, Vector4<T> > {
  typedef SpecializedVector<4, T, Vector4<T> > Base;
public:
  static const Vector4<T> null;

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

  template<int D>
  inline Vector4(const Vector<D, T>& source)
    : Base(source)
  {
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

template<class T>
const Vector4<T> Vector4<T>::null = Vector4<T>();

typedef Vector4<float> Vector4f;
typedef Vector4<double> Vector4d;

#endif
