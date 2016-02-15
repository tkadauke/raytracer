#pragma once

#ifdef __SSE3__

#include "core/DivisionByZeroException.h"
#include <pmmintrin.h>

template<>
class Color<double> {
  typedef double ComponentsType[3];

public:
  typedef double Component;
  
  static const Color<double>& black();
  static const Color<double>& white();
  
  inline Color() {
    m_vector[0] = _mm_setzero_pd();
    m_vector[1] = _mm_setzero_pd();
  }

  inline Color(const ComponentsType& cells) {
    m_vector[0] = _mm_set_pd(cells[1], cells[0]);
    m_vector[1] = _mm_set_sd(cells[2]);
  }
  
  inline Color(const double& r, const double& g, const double& b) {
    m_vector[0] = _mm_set_pd(g, r);
    m_vector[1] = _mm_set_sd(b);
  }

  static Color<double> fromRGB(unsigned int r, unsigned int g, unsigned int b) {
    return Color(double(r) / 255.0, double(g) / 255.0, double(b) / 255.0);
  }

  inline const double& component(int index) const { return m_components[index]; }
  inline void setComponent(int index, const double& value) { m_components[index] = value; }

  inline double& operator[](int index) { return m_components[index]; }
  inline const double& operator[](int index) const { return m_components[index]; }
  
  inline const double& r() const { return component(0); }
  inline const double& g() const { return component(1); }
  inline const double& b() const { return component(2); }

  inline Color<double> operator+(const Color<double>& other) const {
    Color<double> result;
    result.m_vector[0] = _mm_add_pd(m_vector[0], other.m_vector[0]);
    result.m_vector[1] = _mm_add_sd(m_vector[1], other.m_vector[1]);
    return result;
  }

  inline Color<double>& operator+=(const Color<double>& other) {
    m_vector[0] = _mm_add_pd(m_vector[0], other.m_vector[0]);
    m_vector[1] = _mm_add_sd(m_vector[1], other.m_vector[1]);
    return *this;
  }

  inline Color<double> operator-(const Color<double>& other) const {
    Color<double> result;
    result.m_vector[0] = _mm_sub_pd(m_vector[0], other.m_vector[0]);
    result.m_vector[1] = _mm_sub_sd(m_vector[1], other.m_vector[1]);
    return result;
  }

  inline Color<double> operator/(const double& factor) const {
    if (factor == 0.0)
      throw DivisionByZeroException(__FILE__, __LINE__);

    double recip = 1.0 / factor;
    return *this * recip;
  }

  inline Color<double> operator*(const double& factor) const {
    __m128d f = _mm_set1_pd(factor);
    Color<double> result;
    result.m_vector[0] = _mm_mul_pd(m_vector[0], f);
    result.m_vector[1] = _mm_mul_sd(m_vector[1], f);
    return result;
  }
  
  inline Color<double> operator*(const Color<double>& intensity) const {
    Color<double> result;
    result.m_vector[0] = _mm_mul_pd(m_vector[0], intensity.m_vector[0]);
    result.m_vector[1] = _mm_mul_sd(m_vector[1], intensity.m_vector[1]);
    return result;
  }
  
  inline bool operator==(const Color<double>& other) const {
    for (int i = 0; i != 3; ++i) {
      if (component(i) != other.component(i))
        return false;
    }
    return true;
  }

  inline bool operator!=(const Color<double>& other) const {
    return !(*this == other);
  }

  inline unsigned char rInt() const { return std::min(unsigned(r() * 255), 255u); }
  inline unsigned char gInt() const { return std::min(unsigned(g() * 255), 255u); }
  inline unsigned char bInt() const { return std::min(unsigned(b() * 255), 255u); }
  
  inline unsigned int rgb() const {
    typedef union {
      __m128i vec;
      int coord[2];
    } Half;
    Half first, second;

    __m128d f = _mm_set1_pd(255);
    first.vec = _mm_cvtpd_epi32(_mm_mul_pd(m_vector[0], f));
    second.vec = _mm_cvtpd_epi32(_mm_mul_sd(m_vector[1], f));
    
    return std::min(unsigned(first.coord[0]), 255u) << 16 |
           std::min(unsigned(first.coord[1]), 255u) << 8 |
           std::min(unsigned(second.coord[0]), 255u);
  }
  
private:
  union {
    double m_components[3];
    __m128d m_vector[2];
  };
};

#endif
