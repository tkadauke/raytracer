#ifndef VECTOR4D_H
#define VECTOR4D_H

#ifdef __SSE3__

#include <pmmintrin.h>

template<>
class Vector4<double> : public Vector<4, double, __m128d> {
  typedef double CellsType[4];
public:
  static const int Dim = 4;
  
  static const Vector4<double>& null();
  static const Vector4<double>& epsilon();
  static const Vector4<double>& undefined();
  static const Vector4<double>& minusInfinity();
  static const Vector4<double>& plusInfinity();

  inline Vector4() {
    m_vector[0] = _mm_setzero_pd();
    m_vector[1] = _mm_set_pd(1.0, 0.0);
  }

  inline Vector4(const double& x, const double& y, const double& z = 0, const double& w = 1) {
    m_vector[0] = _mm_set_pd(y, x);
    m_vector[1] = _mm_set_pd(w, z);
  }
  
  inline Vector4(const CellsType& cells) {
    m_vector[0] = _mm_set_pd(cells[1], cells[0]);
    m_vector[1] = _mm_set_pd(cells[3], cells[2]);
  }

  template<class T>
  inline Vector4(const Vector3<T>& source) {
    m_coordinates[0] = source.coordinate(0);
    m_coordinates[1] = source.coordinate(1);
    m_coordinates[2] = source.coordinate(2);
    m_coordinates[3] = 1.0;
  }

  template<int D, class C, class V>
  inline Vector4(const Vector<D, C, V>& source) {
    m_vector[0] = _mm_setzero_pd();
    m_vector[1] = _mm_set_pd(1.0, 0.0);
    for (int i = 0; i != Dim && i != D; ++i)
      m_coordinates[i] = source.coordinate(i);
  }

  inline double x() const { return m_coordinates[0]; }
  inline double y() const { return m_coordinates[1]; }
  inline double z() const { return m_coordinates[2]; }
  inline double w() const { return m_coordinates[3]; }

  inline Vector4<double> operator+(const Vector4<double>& other) const {
    Vector4<double> result;
    result.m_vector[0] = _mm_add_pd(m_vector[0], other.m_vector[0]);
    result.m_vector[1] = _mm_add_pd(m_vector[1], other.m_vector[1]);
    return result;
  }

  inline Vector4<double> operator-(const Vector4<double>& other) const {
    Vector4<double> result;
    result.m_vector[0] = _mm_sub_pd(m_vector[0], other.m_vector[0]);
    result.m_vector[1] = _mm_sub_pd(m_vector[1], other.m_vector[1]);
    return result;
  }

  inline Vector4<double> operator-() const {
    Vector4<double> result;
    for (int i = 0; i != Dim; ++i) {
      result.setCoordinate(i, - coordinate(i));
    }
    return result;
  }

  inline Vector4<double> operator/(const double& factor) const {
    if (factor == double())
      throw DivisionByZeroException(__FILE__, __LINE__);

    double recip = 1.0 / factor;
    return *this * recip;
  }
  
  inline double operator*(const Vector4<double>& other) const {
    typedef union {
      __m128d vec;
      double coord[2];
    } Half;
    Half first, second;
    first.vec = _mm_mul_pd(m_vector[0], other.m_vector[0]);
    second.vec = _mm_mul_pd(m_vector[1], other.m_vector[1]);
    return first.coord[0] + first.coord[1] + second.coord[0] + second.coord[1];
  }

  inline Vector4<double> operator*(const double& factor) const {
    __m128d f = _mm_set1_pd(factor);
    Vector4<double> result;
    result.m_vector[0] = _mm_mul_pd(m_vector[0], f);
    result.m_vector[1] = _mm_mul_pd(m_vector[1], f);
    return result;
  }
  
  inline bool operator==(const Vector4<double>& other) const {
    if (&other == this)
      return true;
    for (int i = 0; i != Dim; ++i) {
      if (coordinate(i) != other.coordinate(i))
        return false;
    }
    return true;
  }

  inline bool operator!=(const Vector4<double>& other) const {
    return !(*this == other);
  }
  
  inline Vector4<double>& operator+=(const Vector4<double>& other) {
    m_vector[0] = _mm_add_pd(m_vector[0], other.m_vector[0]);
    m_vector[1] = _mm_add_pd(m_vector[1], other.m_vector[1]);
    return *this;
  }

  inline Vector4<double>& operator-=(const Vector4<double>& other) {
    m_vector[0] = _mm_sub_pd(m_vector[0], other.m_vector[0]);
    m_vector[1] = _mm_sub_pd(m_vector[1], other.m_vector[1]);
    return *this;
  }

  inline Vector4<double>& operator*=(const double& factor) {
    __m128d f = _mm_set1_pd(factor);
    m_vector[0] = _mm_mul_pd(m_vector[0], f);
    m_vector[1] = _mm_mul_pd(m_vector[1], f);
    return *this;
  }

  inline Vector4<double>& operator/=(const double& factor) {
    if (factor == double())
      throw DivisionByZeroException(__FILE__, __LINE__);

    double recip = 1.0 / factor;
    return this->operator*=(recip);
  }

  inline double length() const {
    return std::sqrt(*this * *this);
  }

  inline void normalize() {
    *this /= length();
  }

  inline Vector4<double> normalized() {
    return *this / length();
  }

  inline Vector3<double> homogenized() const {
    return Vector3<double>(*this) / w();
  }
};

#endif

#endif
