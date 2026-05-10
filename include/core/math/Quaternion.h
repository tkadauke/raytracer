#pragma once

#include "core/InequalityOperator.h"
#include <cmath>
#include <functional>

template<class T>
class Quaternion : public InequalityOperator<Quaternion<T>> {
public:
  inline Quaternion()
    : m_w(T(1)),
      m_x(T()),
      m_y(T()),
      m_z(T())
  {
  }
  
  inline Quaternion(const T& w, const T& x, const T& y, const T& z)
    : m_w(w),
      m_x(x),
      m_y(y),
      m_z(z)
  {
  }
  
  template<class Vector>
  inline explicit Quaternion(const T& scalar, const Vector& vector)
    : m_w(scalar),
      m_x(vector[0]),
      m_y(vector[1]),
      m_z(vector[2])
  {
  }
  
  inline const T& w() const {
    return m_w;
  }
  
  inline const T& x() const {
    return m_x;
  }
  
  inline const T& y() const {
    return m_y;
  }
  
  inline const T& z() const {
    return m_z;
  }
  
  inline bool operator==(const Quaternion<T>& other) const {
    return m_w == other.w() && m_x == other.x() && m_y == other.y() && m_z == other.z();
  }
  
  inline Quaternion<T> operator*(const T& scalar) const {
    return Quaternion<T>(m_w * scalar, m_x * scalar, m_y * scalar, m_z * scalar);
  }
  
  inline Quaternion<T> operator/(const T& scalar) const {
    return Quaternion<T>(m_w / scalar, m_x / scalar, m_y / scalar, m_z / scalar);
  }
  
  inline T length() const {
    return std::sqrt(m_w*m_w + m_x*m_x + m_y*m_y + m_z*m_z);
  }
  
  inline Quaternion<T> normalized() const {
    return *this / length();
  }

  inline Quaternion<T> operator*(const Quaternion<T>& other) const {
    return Quaternion<T>(
      m_w * other.w() - m_x * other.x() - m_y * other.y() - m_z * other.z(),
      m_w * other.x() + m_x * other.w() + m_y * other.z() - m_z * other.y(),
      m_w * other.y() - m_x * other.z() + m_y * other.w() + m_z * other.x(),
      m_w * other.z() + m_x * other.y() - m_y * other.x() + m_z * other.w()
    );
  }

private:
  T m_w, m_x, m_y, m_z;
};

template<class T>
std::ostream& operator<<(std::ostream& os, const Quaternion<T>& quaternion) {
  os << "[" << quaternion.w() << ", " << quaternion.x() << " " << quaternion.y() << " " << quaternion.z() << "]";
  return os;
}

typedef Quaternion<float> Quaternionf;
typedef Quaternion<double> Quaterniond;

namespace std {
  template<class T>
  struct hash<Quaternion<T>> {
    size_t operator()(const Quaternion<T>& q) const noexcept {
      size_t seed = 0;
      hash<T> h;
      seed ^= h(q.w()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      seed ^= h(q.x()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      seed ^= h(q.y()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      seed ^= h(q.z()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      return seed;
    }
  };
}

#if __cplusplus >= 202002L && __has_include(<format>)
#include <format>
#ifdef __cpp_lib_format
namespace std {
  template<class T>
  struct formatter<Quaternion<T>> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    auto format(const Quaternion<T>& q, format_context& ctx) const {
      return format_to(ctx.out(), "[{}, {} {} {}]", q.w(), q.x(), q.y(), q.z());
    }
  };
}
#endif
#endif
