#pragma once

#include <cmath>

/**
  * This class represents an angle. It is a simple wrapper around a float or
  * double (type parameter T), but it makes angular calculations explicit and
  * type safe. Angles can be constructed and converted to one of several units:
  * 
  * * Radians
  * * Degrees
  * * Turns
  * 
  * Internally, the angle is stored in radians, which makes calculations easy
  * for all the functions that use radians.
  */
template<class T>
class Angle {
public:
  /**
    * Default constructor. Constructs a zero angle.
    */
  inline Angle()
    : m_radians()
  {
  }
  
  /**
    * Constructs an angle from degrees. Internally, the angle is stored in
    * radians, with \f$r = d \times \frac{2\pi}{360}\f$.
    */
  static Angle<T> fromDegrees(const T& degrees) {
    return Angle<T>(degrees * 0.01745329251996);
  }
  
  /**
    * Constructs an angle from radians.
    */
  static Angle<T> fromRadians(const T& radians) {
    return Angle<T>(radians);
  }
  
  /**
    * Constructs an angle from degrees. Internally, the angle is stored in
    * radians, with \f$r = t \times 2\pi\f$.
    */
  static Angle<T> fromTurns(const T& turns) {
    return Angle<T>(turns * 6.28318530717959);
  }
  
  /**
    * Returns the angle in degrees, \f$d = r \times \frac{360}{2\pi}\f$.
    */
  inline T degrees() const {
    return m_radians * 57.29577951308233;
  }
  
  /**
    * Returns the angle in radians.
    */
  inline T radians() const {
    return m_radians;
  }
  
  /**
    * Returns the angle in turns, \f$t = \frac{r}{2\pi}.
    */
  inline T turns() const {
    return m_radians * 0.1591549430919;
  }
  
  /**
    * Returns an angle that is the addition between this angle and the other
    * angle.
    */
  inline Angle<T> operator+(const Angle<T>& other) const {
    return Angle<T>(m_radians + other.radians());
  }
  
  /**
    * Returns an angle that is the subtraction between this angle and the other
    * angle.
    */
  inline Angle<T> operator-(const Angle<T>& other) const {
    return Angle<T>(m_radians - other.radians());
  }
  
  /**
    * Returns the negative of this angle.
    */
  inline Angle<T> operator-() const {
    return Angle<T>(-m_radians);
  }
  
private:
  Angle(const T& radians)
    : m_radians(radians)
  {
  }
  
  T m_radians;
};

template<class T>
std::ostream& operator<<(std::ostream& os, const Angle<T>& angle) {
  os << angle.radians();
  return os;
}

typedef Angle<float> Anglef;
typedef Angle<double> Angled;
