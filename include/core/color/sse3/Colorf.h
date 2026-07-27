#pragma once

#include "core/SimdFeatures.h"
#include <algorithm>
#include <array>
#include <type_traits>

#if RAYTRACER_SIMD_SSE

#include "core/DivisionByZeroException.h"
#include "core/InequalityOperator.h"
#include <xmmintrin.h>

template<>
class Color<float> : public ColorBase<Color<float>, float>, public InequalityOperator<Color<float>> {
  typedef float ComponentsType[3];

public:
  typedef float Component;

  static const Color<float>& black();
  static const Color<float>& white();
  static const Color<float>& red();
  static const Color<float>& green();
  static const Color<float>& blue();

  inline Color() {
    m_vector = _mm_setzero_ps();
  }

  inline explicit Color(const ComponentsType& cells) {
    m_vector = _mm_set_ps(0.0f, cells[2], cells[1], cells[0]);
  }

  template<class Source, std::size_t Size,
           typename = std::enable_if_t<(Size >= 3u) && std::is_convertible_v<Source, float>>>
  inline explicit Color(const Source (&cells)[Size]) {
    m_vector = _mm_set_ps(0.0f, static_cast<float>(cells[2]), static_cast<float>(cells[1]),
                          static_cast<float>(cells[0]));
  }

  template<class Source, std::size_t Size,
           typename = std::enable_if_t<(Size >= 3u) && std::is_convertible_v<Source, float>>>
  inline explicit Color(const std::array<Source, Size>& cells) {
    m_vector = _mm_set_ps(0.0f, static_cast<float>(cells[2]), static_cast<float>(cells[1]),
                          static_cast<float>(cells[0]));
  }

  inline Color(const float& r, const float& g, const float& b) {
    m_vector = _mm_set_ps(0.0f, b, g, r);
  }

  inline Color(const __m128& vec)
      : m_vector(vec) {
  }

  inline const float& component(int index) const {
    return m_components[index];
  }

  inline void setComponent(int index, const float& value) {
    m_components[index] = value;
  }

  inline float& operator[](int index) {
    return m_components[index];
  }

  inline const float& operator[](int index) const {
    return m_components[index];
  }

  inline const float& r() const {
    return component(0);
  }

  inline const float& g() const {
    return component(1);
  }

  inline const float& b() const {
    return component(2);
  }

  inline Color<float> operator+(const Color<float>& other) const {
    return Color<float>(_mm_add_ps(m_vector, other.m_vector));
  }

  inline Color<float>& operator+=(const Color<float>& other) {
    m_vector = _mm_add_ps(m_vector, other.m_vector);
    return *this;
  }

  inline Color<float> operator-(const Color<float>& other) const {
    return Color<float>(_mm_sub_ps(m_vector, other.m_vector));
  }

  inline Color<float> operator/(const float& factor) const {
    if (factor == 0.0f)
      throw DivisionByZeroException(__FILE__, __LINE__);

    float recip = 1.0f / factor;
    return *this * recip;
  }

  inline Color<float> operator*(const float& factor) const {
    return Color<float>(_mm_mul_ps(m_vector, _mm_set1_ps(factor)));
  }

  inline Color<float> operator*(const Color<float>& intensity) const {
    return Color<float>(_mm_mul_ps(m_vector, intensity.m_vector));
  }

  inline bool operator==(const Color<float>& other) const {
    for (int i = 0; i != 3; ++i) {
      if (component(i) != other.component(i))
        return false;
    }
    return true;
  }

  inline unsigned int rgb() const {
    typedef union {
      __m128i vec;
      int coord[3];
    } IntVector;
    IntVector vec;

    vec.vec = _mm_cvtps_epi32(_mm_mul_ps(m_vector, _mm_set1_ps(255.0f)));

    return std::min(unsigned(vec.coord[0]), 255u) << 16 |
           std::min(unsigned(vec.coord[1]), 255u) << 8 | std::min(unsigned(vec.coord[2]), 255u);
  }

private:
  union {
    float m_components[3];
    __m128 m_vector;
  };
};

#endif
