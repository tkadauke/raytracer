#pragma once

#ifdef __SSE__

#include "core/DivisionByZeroException.h"
#include "core/InequalityOperator.h"
#include <xmmintrin.h>

template<>
class Color<float> : public InequalityOperator<Color<float>> {
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
  
  inline Color(const float& r, const float& g, const float& b) {
    m_vector = _mm_set_ps(0.0f, b, g, r);
  }
  
  inline Color(const __m128& vec)
    : m_vector(vec)
  {
  }

  inline static Color<float> fromRGB(unsigned int r, unsigned int g, unsigned int b) {
    return Color(float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f);
  }

  inline static Color<float> fromCMYK(const float& c, const float& m, const float& y, const float& k) {
    return Color(
      (1.0f - c) * (1.0f - k),
      (1.0f - m) * (1.0f - k),
      (1.0f - y) * (1.0f - k)
    );
  }

  inline static Color<float> fromHSV(unsigned int h, const float& s, const float& v) {
    auto c = v * s;
    auto x = c * (1.0f - std::abs((int(h) / 60) % 2 - 1));
    auto m = v - c;
    
    if (h < 60) {
      return Color(c + m, x + m,     m);
    } else if (60 <= h && h < 120) {
      return Color(x + m, c + m,     m);
    } else if (120 <= h && h < 180) {
      return Color(    m, c + m, x + m);
    } else if (180 <= h && h < 240) {
      return Color(    m, x + m, c + m);
    } else if (240 <= h && h < 300) {
      return Color(x + m,     m, c + m);
    } else {
      return Color(c + m,     m, x + m);
    }
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

  inline float k() const {
    return 1.0f - max();
  }
  
  inline float c() const {
    auto w = (1.0f - k());
    if (w == 0)
      return 0.0f;
    return (w - r()) / w;
  }
  
  inline float m() const {
    auto w = (1.0f - k());
    if (w == 0)
      return 0.0f;
    return (w - g()) / w;
  }
  
  inline float y() const {
    auto w = (1.0f - k());
    if (w == 0)
      return 0.0f;
    return (w - b()) / w;
  }
  
  inline unsigned int h() const {
    auto cmax = max();
    auto delta = cmax - min();
    
    int result;
    if (delta == 0) {
      return 0;
    } else if (cmax == r()) {
      result = 60 * ((g() - b()) / delta);
    } else if (cmax == g()) {
      result = 60 * ((b() - r()) / delta + 2);
    } else {
      result = 60 * ((r() - g()) / delta + 4);
    }
    return unsigned(result + 720) % 360;
  }
  
  inline float s() const {
    auto cmax = max();
    auto delta = cmax - min();
    
    if (cmax == 0) {
      return 0;
    } else {
      return delta / cmax;
    }
  }
  
  inline float v() const {
    return max();
  }
  
  inline float max() const {
    return std::max(r(), std::max(g(), b()));
  }
  
  inline float min() const {
    return std::min(r(), std::min(g(), b()));
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

  inline unsigned char rInt() const {
    return std::min(unsigned(r() * 255), 255u);
  }
  
  inline unsigned char gInt() const {
    return std::min(unsigned(g() * 255), 255u);
  }
  
  inline unsigned char bInt() const {
    return std::min(unsigned(b() * 255), 255u);
  }
  
  inline unsigned int rgb() const {
    typedef union {
      __m128i vec;
      int coord[3];
    } IntVector;
    IntVector vec;

    vec.vec = _mm_cvtps_epi32(_mm_mul_ps(m_vector, _mm_set1_ps(255.0f)));
    
    return std::min(unsigned(vec.coord[0]), 255u) << 16 |
           std::min(unsigned(vec.coord[1]), 255u) << 8 |
           std::min(unsigned(vec.coord[2]), 255u);
  }
  
private:
  union {
    float m_components[3];
    __m128 m_vector;
  };
};

#endif
