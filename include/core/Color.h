#pragma once

#include <iostream>

#include "core/DivisionByZeroException.h"
#include "core/InequalityOperator.h"

template<class T>
class Color : public InequalityOperator<Color<T>> {
  typedef T ComponentsType[3];

public:
  typedef T Component;
  
  static const Color<T>& black();
  static const Color<T>& white();
  
  inline Color() {
    for (int i = 0; i != 3; ++i) {
      m_components[i] = T();
    }
  }

  inline explicit Color(const ComponentsType& cells) {
    for (int i = 0; i != 3; ++i) {
      m_components[i] = cells[i];
    }
  }
  
  inline Color(const T& r, const T& g, const T& b) {
    m_components[0] = r;
    m_components[1] = g;
    m_components[2] = b;
  }

  static Color<T> fromRGB(unsigned int r, unsigned int g, unsigned int b) {
    return Color(T(r) / 255.0, T(g) / 255.0, T(b) / 255.0);
  }

  inline const T& component(int index) const {
    return m_components[index];
  }
  
  inline void setComponent(int index, const T& value) {
    m_components[index] = value;
  }
  
  inline T& operator[](int index) {
    return m_components[index];
  }
  
  inline const T& operator[](int index) const {
    return m_components[index];
  }
  
  inline const T& r() const {
    return component(0);
  }
  
  inline const T& g() const {
    return component(1);
  }
  
  inline const T& b() const {
    return component(2);
  }
  
  inline Color<T> operator+(const Color<T>& other) const {
    Color<T> result;
    for (int i = 0; i != 3; ++i) {
      result.setComponent(i, component(i) + other.component(i));
    }
    return result;
  }

  inline Color<T>& operator+=(const Color<T>& other) {
    for (int i = 0; i != 3; ++i) {
      setComponent(i, component(i) + other.component(i));
    }
    return *this;
  }

  inline Color<T> operator-(const Color<T>& other) const {
    Color<T> result;
    for (int i = 0; i != 3; ++i) {
      result.setComponent(i, component(i) - other.component(i));
    }
    return result;
  }

  inline Color<T> operator/(const T& factor) const {
    if (factor == T())
      throw DivisionByZeroException(__FILE__, __LINE__);

    T recip = 1.0 / factor;
    return *this * recip;
  }

  inline Color<T> operator*(const T& factor) const {
    Color<T> result;
    for (int i = 0; i != 3; ++i) {
      result.setComponent(i, component(i) * factor);
    }
    return result;
  }
  
  inline Color<T> operator*(const Color<T>& intensity) const {
    Color<T> result;
    for (int i = 0; i != 3; ++i) {
      result.setComponent(i, component(i) * intensity.component(i));
    }
    return result;
  }
  
  inline bool operator==(const Color<T>& other) const {
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
    return rInt() << 16 |
           gInt() << 8 |
           bInt();
  }
  
private:
  T m_components[3];
};

template<class T>
const Color<T>& Color<T>::black() {
  static Color<T> c(0, 0, 0);
  return c;
}

template<class T>
const Color<T>& Color<T>::white() {
  static Color<T> c(1, 1, 1);
  return c;
}

typedef Color<float> Colorf;
typedef Color<double> Colord;

template<class T>
std::ostream& operator<<(std::ostream& os, const Color<T>& color) {
  os << "(";
  for (int i = 0; i != 3; ++i) {
    os << color[i];
    if (i < 2)
      os << ", ";
  }
  os << ")";
  return os;
}

#include "color/sse3/Colorf.h"
#include "color/sse3/Colord.h"
