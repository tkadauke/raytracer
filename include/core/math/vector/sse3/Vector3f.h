#pragma once

#ifdef __SSE__

#include <xmmintrin.h>

template<>
class Vector3<float> : public Vector<3, float, __m128> {
  typedef float CellsType[3];
public:
  static const int Dim = 3;
  
  static const Vector3<float>& null();
  static const Vector3<float>& one();
  static const Vector3<float>& epsilon();
  static const Vector3<float>& undefined();
  static const Vector3<float>& minusInfinity();
  static const Vector3<float>& plusInfinity();

  static const Vector3<float>& right();
  static const Vector3<float>& up();
  static const Vector3<float>& forward();

  inline Vector3() {
    m_vector[0] = _mm_setzero_ps();
  }

  inline Vector3(const float& x, const float& y, const float& z = 0) {
    m_vector[0] = _mm_set_ps(0.0f, z, y, x);
  }
  
  inline Vector3(const CellsType& cells) {
    m_vector[0] = _mm_set_ps(0.0f, cells[2], cells[1], cells[0]);
  }

  inline Vector3(const __m128& vec) {
    m_vector[0] = vec;
  }

  template<class T>
  inline Vector3(const Vector4<T>& source) {
    m_coordinates[0] = source.coordinate(0);
    m_coordinates[1] = source.coordinate(1);
    m_coordinates[2] = source.coordinate(2);
  }

  template<int D, class C, class V>
  inline Vector3(const Vector<D, C, V>& source) {
    m_vector[0] = _mm_setzero_ps();
    for (int i = 0; i != Dim && i != D; ++i)
      m_coordinates[i] = source.coordinate(i);
  }

  inline float x() const { return m_coordinates[0]; }
  inline float y() const { return m_coordinates[1]; }
  inline float z() const { return m_coordinates[2]; }

  inline Vector3<float> operator+(const Vector3<float>& other) const {
    return Vector3<float>(_mm_add_ps(m_vector[0], other.m_vector[0]));
  }

  inline Vector3<float> operator-(const Vector3<float>& other) const {
    return Vector3<float>(_mm_sub_ps(m_vector[0], other.m_vector[0]));
  }

  inline Vector3<float> operator-() const {
    Vector3<float> result;
    for (int i = 0; i != Dim; ++i) {
      result.setCoordinate(i, - coordinate(i));
    }
    return result;
  }

  inline Vector3<float> operator/(const float& factor) const {
    if (factor == float())
      throw DivisionByZeroException(__FILE__, __LINE__);

    float recip = 1.0 / factor;
    return *this * recip;
  }

  inline float operator*(const Vector3<float>& other) const {
    typedef union {
      __m128 vec;
      float coord[3];
    } Vec;
    Vec vec;
    vec.vec = _mm_mul_ps(m_vector[0], other.m_vector[0]);
    return vec.coord[0] + vec.coord[1] + vec.coord[2];
  }

  inline Vector3<float> operator*(const float& factor) const {
    return Vector3<float>(_mm_mul_ps(m_vector[0], _mm_set1_ps(factor)));
  }
  
  inline Vector3<float> crossProduct(const Vector3<float>& other) const {
    return Vector3<float>(y() * other.z() - z() * other.y(),
                          z() * other.x() - x() * other.z(),
                          x() * other.y() - y() * other.x());
  }
  
  inline Vector3<float> operator^(const Vector3<float>& other) const {
    return Vector3<float>(y() * other.z() - z() * other.y(),
                          z() * other.x() - x() * other.z(),
                          x() * other.y() - y() * other.x());
  }
  
  inline bool operator==(const Vector3<float>& other) const {
    if (&other == this)
      return true;
    for (int i = 0; i != Dim; ++i) {
      if (coordinate(i) != other.coordinate(i))
        return false;
    }
    return true;
  }

  inline bool operator!=(const Vector3<float>& other) const {
    return !(*this == other);
  }
  
  inline Vector3<float>& operator+=(const Vector3<float>& other) {
    m_vector[0] = _mm_add_ps(m_vector[0], other.m_vector[0]);
    return *this;
  }

  inline Vector3<float>& operator-=(const Vector3<float>& other) {
    m_vector[0] = _mm_sub_ps(m_vector[0], other.m_vector[0]);
    return *this;
  }

  inline Vector3<float>& operator*=(const float& factor) {
    m_vector[0] = _mm_mul_ps(m_vector[0], _mm_set1_ps(factor));
    return *this;
  }

  inline Vector3<float>& operator/=(const float& factor) {
    if (factor == float())
      throw DivisionByZeroException(__FILE__, __LINE__);

    float recip = 1.0 / factor;
    return this->operator*=(recip);
  }

  inline float length() const {
    return std::sqrt(*this * *this);
  }
  
  inline void normalize() {
    *this /= length();
  }

  inline Vector3<float> normalized() {
    return *this / length();
  }
};

#endif
