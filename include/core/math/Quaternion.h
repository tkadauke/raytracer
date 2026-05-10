#pragma once

#include "core/InequalityOperator.h"
#include "core/math/Matrix.h"
#include <cmath>

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
    return m_w == other.m_w && m_x == other.m_x && m_y == other.m_y && m_z == other.m_z;
  }

  inline Quaternion<T> operator+(const Quaternion<T>& other) const {
    return Quaternion<T>(m_w + other.m_w, m_x + other.m_x, m_y + other.m_y, m_z + other.m_z);
  }

  inline Quaternion<T> operator-(const Quaternion<T>& other) const {
    return Quaternion<T>(m_w - other.m_w, m_x - other.m_x, m_y - other.m_y, m_z - other.m_z);
  }

  inline Quaternion<T> operator*(const T& scalar) const {
    return Quaternion<T>(m_w * scalar, m_x * scalar, m_y * scalar, m_z * scalar);
  }

  inline Quaternion<T> operator/(const T& scalar) const {
    return Quaternion<T>(m_w / scalar, m_x / scalar, m_y / scalar, m_z / scalar);
  }

  inline Quaternion<T> operator*(const Quaternion<T>& other) const {
    return Quaternion<T>(
      m_w * other.m_w - m_x * other.m_x - m_y * other.m_y - m_z * other.m_z,
      m_w * other.m_x + m_x * other.m_w + m_y * other.m_z - m_z * other.m_y,
      m_w * other.m_y - m_x * other.m_z + m_y * other.m_w + m_z * other.m_x,
      m_w * other.m_z + m_x * other.m_y - m_y * other.m_x + m_z * other.m_w
    );
  }

  inline T dot(const Quaternion<T>& other) const {
    return m_w * other.m_w + m_x * other.m_x + m_y * other.m_y + m_z * other.m_z;
  }

  inline T lengthSquared() const {
    return m_w*m_w + m_x*m_x + m_y*m_y + m_z*m_z;
  }

  inline T length() const {
    return std::sqrt(lengthSquared());
  }

  inline Quaternion<T> normalized() const {
    return *this / length();
  }

  inline Quaternion<T> conjugate() const {
    return Quaternion<T>(m_w, -m_x, -m_y, -m_z);
  }

  // For unit quaternions, inverse() == conjugate(). For general q: q^-1 = q* / |q|^2.
  inline Quaternion<T> inverse() const {
    return conjugate() / lengthSquared();
  }

  // Rotates v by this quaternion. Assumes unit quaternion.
  // Uses the optimized formula: t = 2*(q.xyz x v); v' = v + w*t + q.xyz x t.
  inline Vector3<T> rotate(const Vector3<T>& v) const {
    Vector3<T> qvec(m_x, m_y, m_z);
    Vector3<T> t = qvec.crossProduct(v) * T(2);
    return v + t * m_w + qvec.crossProduct(t);
  }

  // Creates a rotation quaternion for angle (radians) around a normalized axis.
  inline static Quaternion<T> fromAxisAngle(const Vector3<T>& axis, const T& angle) {
    T s = std::sin(angle / T(2));
    return Quaternion<T>(std::cos(angle / T(2)), axis.x() * s, axis.y() * s, axis.z() * s);
  }

  // Creates a quaternion from ZYX Euler angles in radians: roll (rx around X),
  // pitch (ry around Y), yaw (rz around Z). Equivalent to Rz*Ry*Rx.
  inline static Quaternion<T> fromEulerAngles(const T& rx, const T& ry, const T& rz) {
    T cx = std::cos(rx / T(2)), sx = std::sin(rx / T(2));
    T cy = std::cos(ry / T(2)), sy = std::sin(ry / T(2));
    T cz = std::cos(rz / T(2)), sz = std::sin(rz / T(2));
    return Quaternion<T>(
      cz*cy*cx + sz*sy*sx,
      cz*cy*sx - sz*sy*cx,
      cz*sy*cx + sz*cy*sx,
      sz*cy*cx - cz*sy*sx
    );
  }

  // Extracts ZYX Euler angles in radians as (roll, pitch, yaw). Inverse of fromEulerAngles.
  inline Vector3<T> toEulerAngles() const {
    T rx = std::atan2(T(2) * (m_w*m_x + m_y*m_z), T(1) - T(2) * (m_x*m_x + m_y*m_y));
    T sinp = std::min(T(1), std::max(T(-1), T(2) * (m_w*m_y - m_z*m_x)));
    T ry = std::asin(sinp);
    T rz = std::atan2(T(2) * (m_w*m_z + m_x*m_y), T(1) - T(2) * (m_y*m_y + m_z*m_z));
    return Vector3<T>(rx, ry, rz);
  }

  // Converts to a 3x3 rotation matrix. Assumes unit quaternion.
  inline Matrix3<T> toMatrix3() const {
    T xx = m_x*m_x, yy = m_y*m_y, zz = m_z*m_z;
    T xy = m_x*m_y, xz = m_x*m_z, yz = m_y*m_z;
    T wx = m_w*m_x, wy = m_w*m_y, wz = m_w*m_z;
    return Matrix3<T>(
      T(1)-T(2)*(yy+zz), T(2)*(xy-wz),      T(2)*(xz+wy),
      T(2)*(xy+wz),      T(1)-T(2)*(xx+zz), T(2)*(yz-wx),
      T(2)*(xz-wy),      T(2)*(yz+wx),      T(1)-T(2)*(xx+yy)
    );
  }

  // Converts to a 4x4 rotation matrix with identity translation. Assumes unit quaternion.
  inline Matrix4<T> toMatrix4() const {
    T xx = m_x*m_x, yy = m_y*m_y, zz = m_z*m_z;
    T xy = m_x*m_y, xz = m_x*m_z, yz = m_y*m_z;
    T wx = m_w*m_x, wy = m_w*m_y, wz = m_w*m_z;
    return Matrix4<T>(
      T(1)-T(2)*(yy+zz), T(2)*(xy-wz),      T(2)*(xz+wy),      T(),
      T(2)*(xy+wz),      T(1)-T(2)*(xx+zz), T(2)*(yz-wx),      T(),
      T(2)*(xz-wy),      T(2)*(yz+wx),      T(1)-T(2)*(xx+yy), T(),
      T(),               T(),               T(),               T(1)
    );
  }

  // Normalized linear interpolation — fast but only approximately constant angular speed.
  inline static Quaternion<T> nlerp(const Quaternion<T>& a, const Quaternion<T>& b, const T& t) {
    return (a * (T(1) - t) + b * t).normalized();
  }

  // Spherical linear interpolation — exact constant angular speed along the great arc.
  // Falls back to nlerp when a and b are nearly identical to avoid division by near-zero.
  inline static Quaternion<T> slerp(const Quaternion<T>& a, const Quaternion<T>& b, const T& t) {
    T cosTheta = a.dot(b);
    Quaternion<T> b2 = b;
    if (cosTheta < T(0)) {
      cosTheta = -cosTheta;
      b2 = b * T(-1);
    }
    if (cosTheta > T(1) - T(1e-6)) {
      return nlerp(a, b2, t);
    }
    T theta = std::acos(cosTheta);
    T sinTheta = std::sin(theta);
    return a * (std::sin((T(1) - t) * theta) / sinTheta) + b2 * (std::sin(t * theta) / sinTheta);
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
