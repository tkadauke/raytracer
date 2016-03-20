#pragma once

#ifdef __SSE__

#include <xmmintrin.h>

template<>
class Vector4<float> : public Vector<4, float, __m128, Vector4<float>> {
  typedef float CellsType[4];
public:
  static const int Dim = 4;
  
  static const Vector4<float>& null();
  static const Vector4<float>& epsilon();
  static const Vector4<float>& undefined();
  static const Vector4<float>& minusInfinity();
  static const Vector4<float>& plusInfinity();

  inline Vector4() {
    m_vector[0] = _mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f);
  }

  inline Vector4(const float& x, const float& y, const float& z = 0, const float& w = 1) {
    m_vector[0] = _mm_set_ps(w, z, y, x);
  }
  
  inline Vector4(const CellsType& cells) {
    m_vector[0] = _mm_set_ps(cells[3], cells[2], cells[1], cells[0]);
  }

  template<class T>
  inline Vector4(const Vector3<T>& source) {
    m_coordinates[0] = source.coordinate(0);
    m_coordinates[1] = source.coordinate(1);
    m_coordinates[2] = source.coordinate(2);
    m_coordinates[3] = 1.0;
  }

  template<int D, class C, class S, class V>
  inline Vector4(const Vector<D, C, S, V>& source) {
    m_vector[0] = _mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f);
    for (int i = 0; i != Dim && i != D; ++i)
      m_coordinates[i] = source.coordinate(i);
  }

  inline float x() const {
    return m_coordinates[0];
  }
  
  inline float y() const {
    return m_coordinates[1];
  }
  
  inline float z() const {
    return m_coordinates[2];
  }
  
  inline float w() const {
    return m_coordinates[3];
  }
  
  inline Vector4<float> operator+(const Vector4<float>& other) const {
    return Vector4<float>(_mm_add_ps(m_vector[0], other.m_vector[0]));
  }

  inline Vector4<float> operator-(const Vector4<float>& other) const {
    return Vector4<float>(_mm_sub_ps(m_vector[0], other.m_vector[0]));
  }

  inline Vector4<float> operator-() const {
    return Vector4<float>(_mm_sub_ps(_mm_setzero_ps(), m_vector[0]));
  }

  inline float operator*(const Vector4<float>& other) const {
    typedef union {
      __m128 vec;
      float coord[4];
    } Vec;
    Vec vec;
    vec.vec = _mm_mul_ps(m_vector[0], other.m_vector[0]);
    return vec.coord[0] + vec.coord[1] + vec.coord[2] + vec.coord[3];
  }

  inline Vector4<float> operator*(const float& factor) const {
    return Vector4<float>(_mm_mul_ps(m_vector[0], _mm_set1_ps(factor)));
  }
  
  inline Vector4<float>& operator+=(const Vector4<float>& other) {
    m_vector[0] = _mm_add_ps(m_vector[0], other.m_vector[0]);
    return *this;
  }

  inline Vector4<float>& operator-=(const Vector4<float>& other) {
    m_vector[0] = _mm_sub_ps(m_vector[0], other.m_vector[0]);
    return *this;
  }

  inline Vector4<float>& operator*=(const float& factor) {
    m_vector[0] = _mm_mul_ps(m_vector[0], _mm_set1_ps(factor));
    return *this;
  }

  inline Vector3<float> homogenized() const {
    return Vector3<float>(*this) / w();
  }

private:
  inline explicit Vector4(const __m128& vec) {
    m_vector[0] = vec;
  }
};

#endif
