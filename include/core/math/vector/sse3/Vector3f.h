#pragma once

#include "core/SimdFeatures.h"

#if RAYTRACER_SIMD_SSE

#include <array>
#include <type_traits>
#include <xmmintrin.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif

template<>
class Vector3<float> : public Vector<3, float, __m128, Vector3<float>> {
  typedef float CellsType[3];

public:
  static const int Dim = 3;

  static const Vector3<float> null;
  static const Vector3<float> one;
  static const Vector3<float> epsilon;
  static const Vector3<float> undefined;
  static const Vector3<float> minusInfinity;
  static const Vector3<float> plusInfinity;

  static const Vector3<float>& right();
  static const Vector3<float>& up();
  static const Vector3<float>& forward();

  inline Vector3() {
    m_vector[0] = _mm_setzero_ps();
  }

  inline Vector3(const float& x, const float& y, const float& z = 0) {
    m_vector[0] = _mm_set_ps(0.0f, z, y, x);
  }

  inline explicit Vector3(const CellsType& cells) {
    m_vector[0] = _mm_set_ps(0.0f, cells[2], cells[1], cells[0]);
  }

  template<class Source, std::size_t Size,
           typename = std::enable_if_t<(Size >= 3u) && std::is_convertible_v<Source, float>>>
  inline explicit Vector3(const std::array<Source, Size>& cells) {
    m_vector[0] = _mm_set_ps(0.0f, static_cast<float>(cells[2]), static_cast<float>(cells[1]),
                             static_cast<float>(cells[0]));
  }

  template<class T>
  inline Vector3(const Vector4<T>& source) {
    m_vector[0] = _mm_set_ps(0.0f, static_cast<float>(source.z()), static_cast<float>(source.y()),
                             static_cast<float>(source.x()));
  }

  template<int D, class C, class S, class V>
  inline Vector3(const Vector<D, C, S, V>& source) {
    const float x = D > 0 ? static_cast<float>(source.coordinate(0)) : 0.0f;
    const float y = D > 1 ? static_cast<float>(source.coordinate(1)) : 0.0f;
    const float z = D > 2 ? static_cast<float>(source.coordinate(2)) : 0.0f;
    m_vector[0] = _mm_set_ps(0.0f, z, y, x);
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
    return _mm_cvtss_f32(_mm_movehl_ps(m_vector[0], m_vector[0]));
  }

  inline void setZ(const float& value) noexcept {
    float lanes[4];
    _mm_storeu_ps(lanes, m_vector[0]);
    lanes[2] = value;
    m_vector[0] = _mm_loadu_ps(lanes);
  }

  [[nodiscard]] inline Vector3<float> operator+(const Vector3<float>& other) const noexcept {
    return Vector3<float>(_mm_add_ps(m_vector[0], other.m_vector[0]));
  }

  [[nodiscard]] inline Vector3<float> operator-(const Vector3<float>& other) const noexcept {
    return Vector3<float>(_mm_sub_ps(m_vector[0], other.m_vector[0]));
  }

  [[nodiscard]] inline Vector3<float> operator-() const noexcept {
    return Vector3<float>(_mm_sub_ps(_mm_setzero_ps(), m_vector[0]));
  }

  [[nodiscard]] inline float operator*(const Vector3<float>& other) const noexcept {
    const __m128 products = _mm_mul_ps(m_vector[0], other.m_vector[0]);
    return _mm_cvtss_f32(products) +
           _mm_cvtss_f32(_mm_shuffle_ps(products, products, _MM_SHUFFLE(1, 1, 1, 1))) +
           _mm_cvtss_f32(_mm_movehl_ps(products, products));
  }

  [[nodiscard]] inline Vector3<float> operator*(const float& factor) const noexcept {
    return Vector3<float>(_mm_mul_ps(m_vector[0], _mm_set1_ps(factor)));
  }

  [[nodiscard]] inline Vector3<float> crossProduct(const Vector3<float>& other) const noexcept {
    return Vector3<float>(y() * other.z() - z() * other.y(), z() * other.x() - x() * other.z(),
                          x() * other.y() - y() * other.x());
  }

  [[nodiscard]] inline Vector3<float> operator^(const Vector3<float>& other) const noexcept {
    return Vector3<float>(y() * other.z() - z() * other.y(), z() * other.x() - x() * other.z(),
                          x() * other.y() - y() * other.x());
  }

  inline Vector3<float>& operator+=(const Vector3<float>& other) noexcept {
    m_vector[0] = _mm_add_ps(m_vector[0], other.m_vector[0]);
    return *this;
  }

  inline Vector3<float>& operator-=(const Vector3<float>& other) noexcept {
    m_vector[0] = _mm_sub_ps(m_vector[0], other.m_vector[0]);
    return *this;
  }

  inline Vector3<float>& operator*=(const float& factor) noexcept {
    m_vector[0] = _mm_mul_ps(m_vector[0], _mm_set1_ps(factor));
    return *this;
  }

private:
  inline explicit Vector3(const __m128& vec) {
    m_vector[0] = vec;
  }
};

inline const Vector3<float> Vector3<float>::null{0.0f, 0.0f, 0.0f};
inline const Vector3<float> Vector3<float>::one{1.0f, 1.0f, 1.0f};
inline const Vector3<float> Vector3<float>::epsilon{std::numeric_limits<float>::epsilon(),
                                                    std::numeric_limits<float>::epsilon(),
                                                    std::numeric_limits<float>::epsilon()};
inline const Vector3<float> Vector3<float>::undefined{std::numeric_limits<float>::quiet_NaN(),
                                                      std::numeric_limits<float>::quiet_NaN(),
                                                      std::numeric_limits<float>::quiet_NaN()};
inline const Vector3<float> Vector3<float>::minusInfinity{-std::numeric_limits<float>::infinity(),
                                                          -std::numeric_limits<float>::infinity(),
                                                          -std::numeric_limits<float>::infinity()};
inline const Vector3<float> Vector3<float>::plusInfinity{std::numeric_limits<float>::infinity(),
                                                         std::numeric_limits<float>::infinity(),
                                                         std::numeric_limits<float>::infinity()};

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif
