#pragma once

#include "core/math/Constants.h"
#include "core/InequalityOperator.h"

#include <cmath>

/**
  * This class represents an angle. It is a simple wrapper around a float or
  * double (type parameter T), but it makes angular calculations explicit and
  * type safe. Angles can be constructed and converted to one of several units:
  * 
  * * Radians
  * * Degrees
  * * Turns
  * * o'Clock
  * 
  * Internally, the angle is stored in radians, which makes calculations easy
  * for all the functions that use radians.
  * 
  * This class inherits from InequalityOperator, providing operator !=.
  * 
  * @tparam T the floating point type used to store the angle in radians.
  */
template<class T>
class Angle : public InequalityOperator<Angle<T>> {
public:
  /**
    * Default constructor. Constructs a zero angle.
    */
  inline Angle() = default;
  
  /**
    * Copy constructor. Accepts other Angle object.
    */
  inline Angle(const Angle&) = default;
  
  /**
    * Constructs an angle from @p degrees. Internally, the angle is stored in
    * radians, with \f$r = d \times \frac{2\pi}{360}\f$, where \f$d\f$ is
    * @p degrees (the angle in degrees) and \f$r\f$ is the internal angle in
    * radians. The following interactive figure shows the angle in degrees.
    * Click and drag horizontally to change the angle.
    * 
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="angle_from_x.js"></script>
    * <script type="text/javascript" src="angle_from_degrees.js"></script>
    * @endhtmlonly
    */
  inline static Angle<T> fromDegrees(const T& degrees) {
    return Angle<T>(degrees * 0.01745329251996);
  }
  
  /**
    * Constructs an angle from @p radians. The following interactive figure
    * shows the angle in radians. Click and drag horizontally to change the
    * angle.
    * 
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="angle_from_x.js"></script>
    * <script type="text/javascript" src="angle_from_radians.js"></script>
    * @endhtmlonly
    */
  inline static Angle<T> fromRadians(const T& radians) {
    return Angle<T>(radians);
  }
  
  /**
    * Constructs an angle from @p turns. Internally, the angle is stored in
    * radians, with \f$r = t \times 2\pi\f$, where \f$t\f$ is @p turns (the
    * angle in turns) and \f$r\f$ is the internal angle in radians. The
    * following interactive figure shows the angle in turns. Click and drag
    * horizontally to change the angle.
    * 
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="angle_from_x.js"></script>
    * <script type="text/javascript" src="angle_from_turns.js"></script>
    * @endhtmlonly
    */
  inline static Angle<T> fromTurns(const T& turns) {
    return Angle<T>(turns * TAU);
  }
  
  /**
    * Constructs an angle from @p oClock. Internally, the angle is stored in
    * radians, with \f$r = \frac{o \times 2\pi}{12}\f$, where \f$o\f$ is
    * @p oClock (the hour on a clock) and \f$r\f$ is the angle in radians. The
    * following interactive figure shows the angle in hours. Click and drag
    * horizontally to change the angle.
    * 
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="angle_from_x.js"></script>
    * <script type="text/javascript" src="angle_from_clock.js"></script>
    * @endhtmlonly
    */
  inline static Angle<T> fromClock(const T& oClock) {
    return Angle<T>(oClock * 0.5235987755982988730771);
  }
  
  /**
    * @returns the angle in degrees, \f$d = r \times \frac{360}{2\pi}\f$, where
    *   \f$d\f$ is the angle in degrees and \f$r\f$ is the angle in radians.
    */
  inline T degrees() const {
    return m_radians * 57.29577951308233;
  }
  
  /**
    * @returns the angle in radians.
    */
  inline T radians() const {
    return m_radians;
  }
  
  /**
    * @returns the angle in turns, \f$t = \frac{r}{2\pi}\f$, where \f$t\f$ is
    *   the angle in turns and \f$r\f$ is the angle in radians.
    */
  inline T turns() const {
    return m_radians * invTAU;
  }
  
  /**
    * @returns the angle in hours on the clock, \f$o = \frac{12r}{2\pi}\f$,
    *   where \f$o\f$ is the angle in hours on the clock and \f$r\f$ is the
    *   angle in radians.
    */
  inline T oclock() const {
    return m_radians * 1.909859317102744029227;
  }
  
  /**
    * @returns an angle that is the addition between this angle and the @p other
    *   angle.
    */
  inline Angle<T> operator+(const Angle<T>& other) const {
    return Angle<T>(m_radians + other.radians());
  }
  
  /**
    * @returns an angle that is the subtraction between this angle and the
    *   @p other angle.
    */
  inline Angle<T> operator-(const Angle<T>& other) const {
    return Angle<T>(m_radians - other.radians());
  }
  
  /**
    * @returns the negative of this angle.
    */
  inline Angle<T> operator-() const {
    return Angle<T>(-m_radians);
  }
  
  /**
    * @returns an angle that is the multiplication between this angle and the
    *   @p other value.
    */
  template<class F>
  inline Angle<T> operator*(const F& other) const {
    return Angle<T>(m_radians * other);
  }
  
  /**
    * @returns true if this angles is equal to @p other by comparing their value
    *   in radians, false otherwise.
    */
  inline bool operator==(const Angle<T>& other) const {
    return m_radians == other.radians();
  }

private:
  inline explicit Angle(const T& radians)
    : m_radians(radians)
  {
  }
  
  T m_radians;
};

/**
  * Outputs the @p angle in radians to the given output stream @p os.
  * 
  * @returns @p os.
  */
template<class T>
std::ostream& operator<<(std::ostream& os, const Angle<T>& angle) {
  os << angle.radians();
  return os;
}

/**
  * @returns the multiple of the @p angle by the value on the @p left.
  */
template<class T, class F>
inline Angle<T> operator*(const F& left, const Angle<T>& angle) {
  return Angle<T>::fromRadians(left * angle.radians());
}

/**
  * Shortcut for a float-precision Angle.
  */
typedef Angle<float> Anglef;

/**
  * Shortcut for a double-precision Angle.
  */
typedef Angle<double> Angled;


/**
  * Constant float angle of one degree. Use it with the multiplication operator
  * as a shortcut to Anglef::fromDegrees().
  * 
  * @code
  * Anglef angle = 90 * Degreef;
  * @endcode
  */
const Anglef Degreef = Anglef::fromDegrees(1);

/**
  * Constant float angle of one radian. Use it with the multiplication operator
  * as a shortcut to Anglef::fromRadians().
  * 
  * @code
  * Anglef angle = 2 * Radianf;
  * @endcode
  */
const Anglef Radianf = Anglef::fromRadians(1);

/**
  * Constant float angle of one turn. Use it with the multiplication operator
  * as a shortcut to Anglef::fromTurns().
  * 
  * @code
  * Anglef angle = 2 * Turnf;
  * @endcode
  */
const Anglef Turnf = Anglef::fromTurns(1);

/**
  * Constant float angle of one hour. Use it with the multiplication operator
  * as a shortcut to Anglef::fromClock().
  * 
  * @code
  * Anglef angle = 2 * oClockf;
  * @endcode
  */
const Anglef oClockf = Anglef::fromClock(1);

/**
  * Constant double angle of one degree. Use it with the multiplication operator
  * as a shortcut to Angled::fromDegrees().
  * 
  * @code
  * Angled angle = 90 * Degreed;
  * @endcode
  */
const Angled Degreed = Angled::fromDegrees(1);

/**
  * Constant double angle of one radian. Use it with the multiplication operator
  * as a shortcut to Angled::fromRadians().
  * 
  * @code
  * Angled angle = 2 * Radiand;
  * @endcode
  */
const Angled Radiand = Angled::fromRadians(1);

/**
  * Constant double angle of one turn. Use it with the multiplication operator
  * as a shortcut to Angled::fromTurns().
  * 
  * @code
  * Angled angle = 2 * Turnd;
  * @endcode
  */
const Angled Turnd = Angled::fromTurns(1);

/**
  * Constant double angle of one hour. Use it with the multiplication operator
  * as a shortcut to Angled::fromClock().
  * 
  * @code
  * Angled angle = 2 * oClockd;
  * @endcode
  */
const Angled oClockd = Angled::fromClock(1);

/**
  * Suffix operator for floating point literals.
  * 
  * @returns an Angle<double> with the degrees from the literal.
  * 
  * @see Angle<double>::fromDegrees().
  */
inline Angled operator "" _degrees(long double value) {
  return Angled::fromDegrees(value);
}

/**
  * Suffix operator for integer literals.
  * 
  * @see Angle<double>::fromDegrees().
  * 
  * @returns an Angle<double> with the degrees from the literal.
  */
inline Angled operator "" _degrees(unsigned long long int value) {
  return Angled::fromDegrees(value);
}

/**
  * Suffix operator for floating point literals.
  * 
  * @see Angle<double>::fromRadians().
  * 
  * @returns an Angle<double> with the radians from the literal.
  */
inline Angled operator "" _radians(long double value) {
  return Angled::fromRadians(value);
}

/**
  * Suffix operator for integer literals.
  * 
  * @see Angle<double>::fromRadians().
  * 
  * @returns an Angle<double> with the radians from the literal.
  */
inline Angled operator "" _radians(unsigned long long int value) {
  return Angled::fromRadians(value);
}

/**
  * Suffix operator for floating point literals.
  * 
  * @see Angle<double>::fromTurns().
  * 
  * @returns an Angle<double> with the turns from the literal.
  */
inline Angled operator "" _turns(long double value) {
  return Angled::fromTurns(value);
}

/**
  * Suffix operator for integer literals.
  * 
  * @see Angle<double>::fromTurns().
  * 
  * @returns an Angle<double> with the turns from the literal.
  */
inline Angled operator "" _turns(unsigned long long int value) {
  return Angled::fromTurns(value);
}

/**
  * Suffix operator for floating point literals.
  * 
  * @see Angle<double>::fromHours().
  * 
  * @returns an Angle<double> with the hours on the clock from the literal.
  */
inline Angled operator "" _oclock(long double value) {
  return Angled::fromClock(value);
}

/**
  * Suffix operator for integer literals.
  * 
  * @see Angle<double>::fromHours().
  * 
  * @returns an Angle<double> with the hours on the clock from the literal.
  */
inline Angled operator "" _oclock(unsigned long long int value) {
  return Angled::fromClock(value);
}
