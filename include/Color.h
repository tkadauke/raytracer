#ifndef COLOR_H
#define COLOR_H

#include <iostream>

#include "core/DivisionByZeroException.h"

template<class T>
class Color {
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

  inline Color(const ComponentsType& cells) {
    for (int i = 0; i != 3; ++i) {
      m_components[i] = cells[i];
    }
  }
  
  inline Color(const T& r, const T& g, const T& b) {
    m_components[0] = r;
    m_components[1] = g;
    m_components[2] = b;
  }

  inline const T& component(int index) const { return m_components[index]; }
  inline void setComponent(int index, const T& value) { m_components[index] = value; }

  inline T& operator[](int index) { return m_components[index]; }
  inline const T& operator[](int index) const { return m_components[index]; }
  
  inline const T& r() const { return component(0); }
  inline const T& g() const { return component(1); }
  inline const T& b() const { return component(2); }

  inline Color<T> operator+(const Color<T>& other) const {
    Color<T> result;
    for (int i = 0; i != 3; ++i) {
      result.setComponent(i, component(i) + other.component(i));
    }
    return result;
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

  inline bool operator!=(const Color<T>& other) const {
    return !(*this == other);
  }
  
  inline unsigned int rgb() const {
    return std::min(unsigned(component(0) * 255), 255u) << 16 |
           std::min(unsigned(component(1) * 255), 255u) << 8 |
           std::min(unsigned(component(2) * 255), 255u);
  }
  
private:
  T m_components[3];
};

template<class T>
const Color<T>& Color<T>::black() {
  Color<T>* c = new Color<T>();
  return *c;
}

template<class T>
const Color<T>& Color<T>::white() {
  Color<T>* c = new Color<T>(1, 1, 1);
  return *c;
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

#endif
