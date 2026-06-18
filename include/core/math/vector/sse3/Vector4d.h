#pragma once

#include "core/SimdFeatures.h"

#if RAYTRACER_SIMD_SSE3

#include <array>
#include <type_traits>
#include <pmmintrin.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif

template<>
class Vector4<double> : public Vector<4, double, __m128d, Vector4<double>> {
  typedef double CellsType[4];

public:
  static const int Dim = 4;

  static const Vector4<double> null;
  static const Vector4<double> epsilon;
  static const Vector4<double> undefined;
  static const Vector4<double> minusInfinity;
  static const Vector4<double> plusInfinity;

  inline Vector4() {
    m_vector[0] = _mm_setzero_pd();
    m_vector[1] = _mm_set_pd(1.0, 0.0);
  }

  inline Vector4(const double& x, const double& y, const double& z = 0, const double& w = 1) {
    m_vector[0] = _mm_set_pd(y, x);
    m_vector[1] = _mm_set_pd(w, z);
  }

  inline explicit Vector4(const CellsType& cells) {
    m_vector[0] = _mm_set_pd(cells[1], cells[0]);
    m_vector[1] = _mm_set_pd(cells[3], cells[2]);
  }

  template<class Source, std::size_t Size,
           typename = std::enable_if_t<(Size >= 4u) && std::is_convertible_v<Source, double>>>
  inline explicit Vector4(const std::array<Source, Size>& cells) {
    m_vector[0] = _mm_set_pd(static_cast<double>(cells[1]), static_cast<double>(cells[0]));
    m_vector[1] = _mm_set_pd(static_cast<double>(cells[3]), static_cast<double>(cells[2]));
  }

  template<class T>
  inline Vector4(const Vector3<T>& source) {
    m_vector[0] = _mm_set_pd(static_cast<double>(source.y()), static_cast<double>(source.x()));
    m_vector[1] = _mm_set_pd(1.0, static_cast<double>(source.z()));
  }

  template<int D, class C, class S, class V>
  inline Vector4(const Vector<D, C, S, V>& source) {
    const double x = D > 0 ? static_cast<double>(source.coordinate(0)) : 0.0;
    const double y = D > 1 ? static_cast<double>(source.coordinate(1)) : 0.0;
    const double z = D > 2 ? static_cast<double>(source.coordinate(2)) : 0.0;
    const double w = D > 3 ? static_cast<double>(source.coordinate(3)) : 1.0;
    m_vector[0] = _mm_set_pd(y, x);
    m_vector[1] = _mm_set_pd(w, z);
  }

  [[nodiscard]] inline double x() const noexcept {
    return _mm_cvtsd_f64(m_vector[0]);
  }

  inline void setX(const double& value) noexcept {
    m_vector[0] = _mm_move_sd(m_vector[0], _mm_set_sd(value));
  }

  [[nodiscard]] inline double y() const noexcept {
    return _mm_cvtsd_f64(_mm_unpackhi_pd(m_vector[0], m_vector[0]));
  }

  inline void setY(const double& value) noexcept {
    double lanes[2];
    _mm_storeu_pd(lanes, m_vector[0]);
    lanes[1] = value;
    m_vector[0] = _mm_loadu_pd(lanes);
  }

  [[nodiscard]] inline double z() const noexcept {
    return _mm_cvtsd_f64(m_vector[1]);
  }

  inline void setZ(const double& value) noexcept {
    m_vector[1] = _mm_move_sd(m_vector[1], _mm_set_sd(value));
  }

  [[nodiscard]] inline double w() const noexcept {
    return _mm_cvtsd_f64(_mm_unpackhi_pd(m_vector[1], m_vector[1]));
  }

  inline void setW(const double& value) noexcept {
    double lanes[2];
    _mm_storeu_pd(lanes, m_vector[1]);
    lanes[1] = value;
    m_vector[1] = _mm_loadu_pd(lanes);
  }

  [[nodiscard]] inline Vector4<double> operator+(const Vector4<double>& other) const noexcept {
    return Vector4<double>(_mm_add_pd(m_vector[0], other.m_vector[0]),
                           _mm_add_pd(m_vector[1], other.m_vector[1]));
  }

  [[nodiscard]] inline Vector4<double> operator-(const Vector4<double>& other) const noexcept {
    return Vector4<double>(_mm_sub_pd(m_vector[0], other.m_vector[0]),
                           _mm_sub_pd(m_vector[1], other.m_vector[1]));
  }

  [[nodiscard]] inline Vector4<double> operator-() const noexcept {
    return Vector4<double>(_mm_sub_pd(_mm_setzero_pd(), m_vector[0]),
                           _mm_sub_pd(_mm_setzero_pd(), m_vector[1]));
  }

  [[nodiscard]] inline double operator*(const Vector4<double>& other) const noexcept {
    const __m128d first = _mm_mul_pd(m_vector[0], other.m_vector[0]);
    const __m128d second = _mm_mul_pd(m_vector[1], other.m_vector[1]);
    return _mm_cvtsd_f64(first) + _mm_cvtsd_f64(_mm_unpackhi_pd(first, first)) +
           _mm_cvtsd_f64(second) + _mm_cvtsd_f64(_mm_unpackhi_pd(second, second));
  }

  [[nodiscard]] inline Vector4<double> operator*(const double& factor) const noexcept {
    __m128d f = _mm_set1_pd(factor);
    return Vector4<double>(_mm_mul_pd(m_vector[0], f), _mm_mul_pd(m_vector[1], f));
  }

  inline Vector4<double>& operator+=(const Vector4<double>& other) noexcept {
    m_vector[0] = _mm_add_pd(m_vector[0], other.m_vector[0]);
    m_vector[1] = _mm_add_pd(m_vector[1], other.m_vector[1]);
    return *this;
  }

  inline Vector4<double>& operator-=(const Vector4<double>& other) noexcept {
    m_vector[0] = _mm_sub_pd(m_vector[0], other.m_vector[0]);
    m_vector[1] = _mm_sub_pd(m_vector[1], other.m_vector[1]);
    return *this;
  }

  inline Vector4<double>& operator*=(const double& factor) noexcept {
    __m128d f = _mm_set1_pd(factor);
    m_vector[0] = _mm_mul_pd(m_vector[0], f);
    m_vector[1] = _mm_mul_pd(m_vector[1], f);
    return *this;
  }

  [[nodiscard]] inline Vector3<double> homogenized() const {
    return Vector3<double>(*this) / w();
  }

private:
  inline explicit Vector4(const __m128d& vec0, const __m128d& vec1) {
    m_vector[0] = vec0;
    m_vector[1] = vec1;
  }
};

inline const Vector4<double> Vector4<double>::null{0.0, 0.0, 0.0, 1.0};
inline const Vector4<double> Vector4<double>::epsilon{
  std::numeric_limits<double>::epsilon(), std::numeric_limits<double>::epsilon(),
  std::numeric_limits<double>::epsilon(), std::numeric_limits<double>::epsilon()};
inline const Vector4<double> Vector4<double>::undefined{
  std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(),
  std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()};
inline const Vector4<double> Vector4<double>::minusInfinity{
  -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
  -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
inline const Vector4<double> Vector4<double>::plusInfinity{
  std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
  std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif
