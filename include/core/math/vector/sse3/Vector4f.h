#pragma once

#include "core/SimdFeatures.h"

#if RAYTRACER_SIMD_SSE

#include <xmmintrin.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif

template<>
class Vector4<float> : public Vector<4, float, __m128, Vector4<float>> {
  typedef float CellsType[4];

public:
  static const int Dim = 4;

  static const Vector4<float> null;
  static const Vector4<float> epsilon;
  static const Vector4<float> undefined;
  static const Vector4<float> minusInfinity;
  static const Vector4<float> plusInfinity;

  inline Vector4() {
    m_vector[0] = _mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f);
  }

  inline Vector4(const float& x, const float& y, const float& z = 0, const float& w = 1) {
    m_vector[0] = _mm_set_ps(w, z, y, x);
  }

  inline explicit Vector4(const CellsType& cells) {
    m_vector[0] = _mm_set_ps(cells[3], cells[2], cells[1], cells[0]);
  }

  template<class T>
  inline Vector4(const Vector3<T>& source) {
    m_vector[0] = _mm_set_ps(1.0f, static_cast<float>(source.z()), static_cast<float>(source.y()),
                             static_cast<float>(source.x()));
  }

  template<int D, class C, class S, class V>
  inline Vector4(const Vector<D, C, S, V>& source) {
    const float x = D > 0 ? static_cast<float>(source.coordinate(0)) : 0.0f;
    const float y = D > 1 ? static_cast<float>(source.coordinate(1)) : 0.0f;
    const float z = D > 2 ? static_cast<float>(source.coordinate(2)) : 0.0f;
    const float w = D > 3 ? static_cast<float>(source.coordinate(3)) : 1.0f;
    m_vector[0] = _mm_set_ps(w, z, y, x);
  }

  [[nodiscard]] inline float x() const noexcept {
    return _mm_cvtss_f32(m_vector[0]);
  }

  inline void setX(const float& value) noexcept {
    m_vector[0] = _mm_move_ss(m_vector[0], _mm_set_ss(value));
  }

  [[nodiscard]] inline float y() const noexcept {
    return _mm_cvtss_f32(_mm_shuffle_ps(m_vector[0], m_vector[0], _MM_SHUFFLE(1, 1, 1, 1)));
  }

  inline void setY(const float& value) noexcept {
    float lanes[4];
    _mm_storeu_ps(lanes, m_vector[0]);
    lanes[1] = value;
    m_vector[0] = _mm_loadu_ps(lanes);
  }

  [[nodiscard]] inline float z() const noexcept {
    return _mm_cvtss_f32(_mm_shuffle_ps(m_vector[0], m_vector[0], _MM_SHUFFLE(2, 2, 2, 2)));
  }

  inline void setZ(const float& value) noexcept {
    float lanes[4];
    _mm_storeu_ps(lanes, m_vector[0]);
    lanes[2] = value;
    m_vector[0] = _mm_loadu_ps(lanes);
  }

  [[nodiscard]] inline float w() const noexcept {
    return _mm_cvtss_f32(_mm_shuffle_ps(m_vector[0], m_vector[0], _MM_SHUFFLE(3, 3, 3, 3)));
  }

  inline void setW(const float& value) noexcept {
    float lanes[4];
    _mm_storeu_ps(lanes, m_vector[0]);
    lanes[3] = value;
    m_vector[0] = _mm_loadu_ps(lanes);
  }

  [[nodiscard]] inline Vector4<float> operator+(const Vector4<float>& other) const noexcept {
    return Vector4<float>(_mm_add_ps(m_vector[0], other.m_vector[0]));
  }

  [[nodiscard]] inline Vector4<float> operator-(const Vector4<float>& other) const noexcept {
    return Vector4<float>(_mm_sub_ps(m_vector[0], other.m_vector[0]));
  }

  [[nodiscard]] inline Vector4<float> operator-() const noexcept {
    return Vector4<float>(_mm_sub_ps(_mm_setzero_ps(), m_vector[0]));
  }

  [[nodiscard]] inline float operator*(const Vector4<float>& other) const noexcept {
    const __m128 products = _mm_mul_ps(m_vector[0], other.m_vector[0]);
    return _mm_cvtss_f32(products) +
           _mm_cvtss_f32(_mm_shuffle_ps(products, products, _MM_SHUFFLE(1, 1, 1, 1))) +
           _mm_cvtss_f32(_mm_shuffle_ps(products, products, _MM_SHUFFLE(2, 2, 2, 2))) +
           _mm_cvtss_f32(_mm_shuffle_ps(products, products, _MM_SHUFFLE(3, 3, 3, 3)));
  }

  [[nodiscard]] inline Vector4<float> operator*(const float& factor) const noexcept {
    return Vector4<float>(_mm_mul_ps(m_vector[0], _mm_set1_ps(factor)));
  }

  inline Vector4<float>& operator+=(const Vector4<float>& other) noexcept {
    m_vector[0] = _mm_add_ps(m_vector[0], other.m_vector[0]);
    return *this;
  }

  inline Vector4<float>& operator-=(const Vector4<float>& other) noexcept {
    m_vector[0] = _mm_sub_ps(m_vector[0], other.m_vector[0]);
    return *this;
  }

  inline Vector4<float>& operator*=(const float& factor) noexcept {
    m_vector[0] = _mm_mul_ps(m_vector[0], _mm_set1_ps(factor));
    return *this;
  }

  [[nodiscard]] inline Vector3<float> homogenized() const {
    return Vector3<float>(*this) / w();
  }

private:
  inline explicit Vector4(const __m128& vec) {
    m_vector[0] = vec;
  }
};

inline const Vector4<float> Vector4<float>::null{0.0f, 0.0f, 0.0f, 1.0f};
inline const Vector4<float> Vector4<float>::epsilon{
  std::numeric_limits<float>::epsilon(), std::numeric_limits<float>::epsilon(),
  std::numeric_limits<float>::epsilon(), std::numeric_limits<float>::epsilon()};
inline const Vector4<float> Vector4<float>::undefined{
  std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
  std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
inline const Vector4<float> Vector4<float>::minusInfinity{
  -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
  -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
inline const Vector4<float> Vector4<float>::plusInfinity{
  std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
  std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif
