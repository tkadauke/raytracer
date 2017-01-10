#pragma once

#include "core/math/Constants.h"
#include "core/InequalityOperator.h"

#include <cmath>

/**
  * This class represents an angle. It is a simple wrapper around a float or
  * double (type parameter T), but it makes angular calculations explicit and
  * type safe. Angles can be constructed and converted to one of several units
  * (See https://en.wikipedia.org/wiki/Angle#Units for more information):
  * 
  * * Radians
  * * Grads
  * * Degrees
  * * Arc minutes
  * * Arc seconds
  * * Hexacontades
  * * Binary degrees
  * * Turns
  * * Quadrants
  * * Sextants
  * * o'Clock
  * * Hours
  * * Compass points
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
    * Constructs an angle from @p arcMinutes. An arc minute is
    * \f$\frac{1}{60}\f$ of a degree.
    */
  inline static Angle<T> fromArcMinutes(const T& arcMinutes) {
    return fromDegrees(arcMinutes / 60.0);
  }
  
  /**
    * Constructs an angle from @p arcSeconds. An arc second is
    * \f$\frac{1}{3600}\f$ of a degree.
    */
  inline static Angle<T> fromArcSeconds(const T& arcSeconds) {
    return fromArcMinutes(arcSeconds / 60.0);
  }
  
  /**
    * Constructs an angle from @p hexacontades. A hexacontade is
    * \f$\frac{1}{60}\f$ of a turn.
    */
  inline static Angle<T> fromHexacontades(const T& hexacontades) {
    return fromTurns(hexacontades / 60.0);
  }
  
  /**
    * Constructs an angle from @p binDegrees. A binary degree is
    * \f$\frac{1}{256}\f$ of a turn. Although the resolution is a bit lower,
    * this allows angles to be stored in 8 bits.
    */
  inline static Angle<T> fromBinaryDegrees(const T& binDegrees) {
    return fromTurns(binDegrees / 256.0);
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
    * Constructs an angle from @p grads. A grad is \f$\frac{1}{400}\f$ of a
    * turn.
    */
  inline static Angle<T> fromGrads(const T& grads) {
    return fromTurns(grads / 400.0);
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
    * Constructs an angle from @p quadrants. A quadrant is \f$\frac{1}{4}\f$ of
    * a turn.
    */
  inline static Angle<T> fromQuadrants(const T& quadrants) {
    return fromTurns(quadrants / 4.0);
  }
  
  /**
    * Constructs an angle from @p sextants. A sextant is \f$\frac{1}{4}\f$ of a
    * turn.
    */
  inline static Angle<T> fromSextants(const T& sextants) {
    return fromTurns(sextants / 6.0);
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
    * Constructs an angle from @p hours. An hour is \f$\frac{1}{24}\f$ of a
    * turn.
    */
  inline static Angle<T> fromHours(const T& hours) {
    return fromTurns(hours / 24.0);
  }
  
  /**
    * Constructs an angle from @p compassPoints. A compass point is
    * \f$\frac{1}{32}\f$ of a turn.
    */
  inline static Angle<T> fromCompassPoints(const T& compassPoints) {
    return fromTurns(compassPoints / 32.0);
  }
  
  /**
    * @returns the angle in degrees, \f$d = r \times \frac{360}{2\pi}\f$, where
    *   \f$d\f$ is the angle in degrees and \f$r\f$ is the angle in radians.
    */
  inline T degrees() const {
    return m_radians * 57.29577951308233;
  }
  
  /**
    * @returns the angle in arc minutes, which is degrees() times 60.
    */
  inline T arcMinutes() const {
    return degrees() * 60.0;
  }
  
  /**
    * @returns the angle in arc seconds, which is degrees() times 3600.
    */
  inline T arcSeconds() const {
    return arcMinutes() * 60.0;
  }
  
  /**
    * @returns the angle in hexacontades, which is turns() times 60.
    */
  inline T hexacontades() const {
    return turns() * 60.0;
  }
  
  /**
    * @returns the angle in binary degrees, which is turns() times 256.
    */
  inline T binaryDegrees() const {
    return turns() * 256.0;
  }
  
  /**
    * @returns the angle in radians.
    */
  inline T radians() const {
    return m_radians;
  }
  
  /**
    * @returns the angle in grads, which is turns() times 400.
    */
  inline T grads() const {
    return turns() * 400.0;
  }
  
  /**
    * @returns the angle in turns, \f$t = \frac{r}{2\pi}\f$, where \f$t\f$ is
    *   the angle in turns and \f$r\f$ is the angle in radians.
    */
  inline T turns() const {
    return m_radians * invTAU;
  }
  
  /**
    * @returns the angle in quadrants, which is turns() times 4.
    */
  inline T quadrants() const {
    return turns() * 4.0;
  }
  
  /**
    * @returns the angle in sextants, which is turns() times 6.
    */
  inline T sextants() const {
    return turns() * 6.0;
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
    * @returns the angle in hours, which is turns() times 24.
    */
  inline T hours() const {
    return turns() * 24.0;
  }
  
  /**
    * @returns the angle in compass points, which is turns() times 32.
    */
  inline T compassPoints() const {
    return turns() * 32.0;
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
  * Constant float angle of one arc minute. Use it with the multiplication
  * operator as a shortcut to Anglef::fromArcMinutes().
  * 
  * @code
  * Anglef angle = 5400 * ArcMinutef;
  * @endcode
  */
const Anglef ArcMinutef = Anglef::fromArcMinutes(1);

/**
  * Constant float angle of one arc second. Use it with the multiplication
  * operator as a shortcut to Anglef::fromArcSeconds().
  * 
  * @code
  * Anglef angle = 324000 * ArcSecondf;
  * @endcode
  */
const Anglef ArcSecondf = Anglef::fromArcSeconds(1);

/**
  * Constant float angle of one hexacontade. Use it with the multiplication
  * operator as a shortcut to Anglef::fromHexacontades().
  * 
  * @code
  * Anglef angle = 15 * Hexacontadef;
  * @endcode
  */
const Anglef Hexacontadef = Anglef::fromHexacontades(1);

/**
  * Constant float angle of one binary degree. Use it with the multiplication
  * operator as a shortcut to Anglef::fromBinaryDegrees().
  * 
  * @code
  * Anglef angle = 64 * BinaryDegreef;
  * @endcode
  */
const Anglef BinaryDegreef = Anglef::fromBinaryDegrees(1);

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
  * Constant float angle of one grad. Use it with the multiplication operator
  * as a shortcut to Anglef::fromGrads().
  * 
  * @code
  * Anglef angle = 100 * Gradf;
  * @endcode
  */
const Anglef Gradf = Anglef::fromGrads(1);

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
  * Constant float angle of one quadrant. Use it with the multiplication
  * operator as a shortcut to Anglef::fromQuadrants().
  * 
  * @code
  * Anglef angle = 1 * Quadrantf;
  * @endcode
  */
const Anglef Quadrantf = Anglef::fromQuadrants(1);

/**
  * Constant float angle of one sextant. Use it with the multiplication operator
  * as a shortcut to Anglef::fromSextants().
  * 
  * @code
  * Anglef angle = 2 * Sextantf;
  * @endcode
  */
const Anglef Sextantf = Anglef::fromSextants(1);

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
  * Constant float angle of one hour. Use it with the multiplication operator
  * as a shortcut to Anglef::fromHours().
  * 
  * @code
  * Anglef angle = 6 * Hourf;
  * @endcode
  */
const Anglef Hourf = Anglef::fromHours(1);

/**
  * Constant float angle of one compass point. Use it with the multiplication
  * operator as a shortcut to Anglef::fromCompassPoints().
  * 
  * @code
  * Anglef angle = 90 * CompassPointf;
  * @endcode
  */
const Anglef CompassPointf = Anglef::fromCompassPoints(1);

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
  * Constant double angle of one arc minute. Use it with the multiplication
  * operator as a shortcut to Angled::fromArcMinutes().
  * 
  * @code
  * Angled angle = 90 * ArcMinuted;
  * @endcode
  */
const Angled ArcMinuted = Angled::fromArcMinutes(1);

/**
  * Constant double angle of one arc second. Use it with the multiplication
  * operator as a shortcut to Angled::fromArcSeconds().
  * 
  * @code
  * Angled angle = 5400 * ArcSecondd;
  * @endcode
  */
const Angled ArcSecondd = Angled::fromArcSeconds(1);

/**
  * Constant double angle of one hexacontade. Use it with the multiplication
  * operator as a shortcut to Angled::fromHexacontades().
  * 
  * @code
  * Angled angle = 324000 * Hexacontaded;
  * @endcode
  */
const Angled Hexacontaded = Angled::fromHexacontades(1);

/**
  * Constant double angle of one binary degree. Use it with the multiplication
  * operator as a shortcut to Angled::fromBinaryDegrees().
  * 
  * @code
  * Angled angle = 64 * Degreed;
  * @endcode
  */
const Angled BinaryDegreed = Angled::fromBinaryDegrees(1);

/**
  * Constant double angle of one radian. Use it with the multiplication operator
  * as a shortcut to Angled::fromRadians().
  * 
  * @code
  * Angled angle = 2 * Angled;
  * @endcode
  */
const Angled Radiand = Angled::fromRadians(1);

/**
  * Constant double angle of one grad. Use it with the multiplication operator
  * as a shortcut to Angled::fromGrads().
  * 
  * @code
  * Angled angle = 90 * Gradd;
  * @endcode
  */
const Angled Gradd = Angled::fromGrads(1);

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
  * Constant double angle of one quadrant. Use it with the multiplication
  * operatoras a shortcut to Angled::fromQuadrants().
  * 
  * @code
  * Angled angle = 1 * Quadrantd;
  * @endcode
  */
const Angled Quadrantd = Angled::fromQuadrants(1);

/**
  * Constant double angle of one sextant. Use it with the multiplication
  * operator as a shortcut to Angled::fromSextants().
  * 
  * @code
  * Angled angle = 90 * Sextantd;
  * @endcode
  */
const Angled Sextantd = Angled::fromSextants(1);

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
  * Constant double angle of one hour. Use it with the multiplication operator
  * as a shortcut to Angled::fromHours().
  * 
  * @code
  * Angled angle = 6 * Hourd;
  * @endcode
  */
const Angled Hourd = Angled::fromHours(1);

/**
  * Constant double angle of one compass point. Use it with the multiplication
  * operator as a shortcut to Angled::fromCompassPoints().
  * 
  * @code
  * Angled angle = 8 * Degreed;
  * @endcode
  */
const Angled CompassPointd = Angled::fromCompassPoints(1);

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
  * Suffix operator for floating point literals.
  * 
  * @returns an Angle<double> with the arc minutes from the literal.
  * 
  * @see Angle<double>::fromArcMinutes().
  */
inline Angled operator "" _arc_minutes(long double value) {
  return Angled::fromArcMinutes(value);
}

/**
  * Suffix operator for floating point literals.
  * 
  * @returns an Angle<double> with the arc seconds from the literal.
  * 
  * @see Angle<double>::fromArcSeconds().
  */
inline Angled operator "" _arc_seconds(long double value) {
  return Angled::fromArcSeconds(value);
}

/**
  * Suffix operator for floating point literals.
  * 
  * @returns an Angle<double> with the hexacontades from the literal.
  * 
  * @see Angle<double>::fromHexacontades().
  */
inline Angled operator "" _hexacontades(long double value) {
  return Angled::fromHexacontades(value);
}

/**
  * Suffix operator for floating point literals.
  * 
  * @returns an Angle<double> with the binary degrees from the literal.
  * 
  * @see Angle<double>::fromBinaryDegrees().
  */
inline Angled operator "" _binary_degrees(long double value) {
  return Angled::fromBinaryDegrees(value);
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
  * Suffix operator for integer literals.
  * 
  * @see Angle<double>::fromArcMinutes().
  * 
  * @returns an Angle<double> with the arc minutes from the literal.
  */
inline Angled operator "" _arc_minutes(unsigned long long int value) {
  return Angled::fromArcMinutes(value);
}

/**
  * Suffix operator for integer literals.
  * 
  * @see Angle<double>::fromArcSeconds().
  * 
  * @returns an Angle<double> with the arc seconds from the literal.
  */
inline Angled operator "" _arc_seconds(unsigned long long int value) {
  return Angled::fromArcSeconds(value);
}

/**
  * Suffix operator for hexacontades.
  * 
  * @see Angle<double>::fromDegrees().
  * 
  * @returns an Angle<double> with the degrees from the literal.
  */
inline Angled operator "" _hexacontades(unsigned long long int value) {
  return Angled::fromHexacontades(value);
}

/**
  * Suffix operator for integer literals.
  * 
  * @see Angle<double>::fromBinaryDegrees().
  * 
  * @returns an Angle<double> with the degrees from the literal.
  */
inline Angled operator "" _binary_degrees(unsigned long long int value) {
  return Angled::fromBinaryDegrees(value);
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
  * Suffix operator for floating point literals.
  * 
  * @see Angle<double>::fromGrad().
  * 
  * @returns an Angle<double> with the grads from the literal.
  */
inline Angled operator "" _grads(long double value) {
  return Angled::fromGrads(value);
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
  * Suffix operator for integer literals.
  * 
  * @see Angle<double>::fromGrads().
  * 
  * @returns an Angle<double> with the grads from the literal.
  */
inline Angled operator "" _grads(unsigned long long int value) {
  return Angled::fromGrads(value);
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
  * Suffix operator for floating point literals.
  * 
  * @see Angle<double>::fromQuadrants().
  * 
  * @returns an Angle<double> with the quadrants from the literal.
  */

inline Angled operator "" _quadrants(long double value) {
  return Angled::fromQuadrants(value);
}

/**
  * Suffix operator for floating point literals.
  * 
  * @see Angle<double>::fromSextants().
  * 
  * @returns an Angle<double> with the turns from the literal.
  */
inline Angled operator "" _sextants(long double value) {
  return Angled::fromSextants(value);
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
  * Suffix operator for integer literals.
  * 
  * @see Angle<double>::fromQuadrants().
  * 
  * @returns an Angle<double> with the quadrants from the literal.
  */
inline Angled operator "" _quadrants(unsigned long long int value) {
  return Angled::fromQuadrants(value);
}

/**
  * Suffix operator for integer literals.
  * 
  * @see Angle<double>::fromSextants().
  * 
  * @returns an Angle<double> with the sextants from the literal.
  */
inline Angled operator "" _sextants(unsigned long long int value) {
  return Angled::fromSextants(value);
}

/**
  * Suffix operator for floating point literals.
  * 
  * @see Angle<double>::fromClock().
  * 
  * @returns an Angle<double> with the hours from the literal.
  */
inline Angled operator "" _oclock(long double value) {
  return Angled::fromClock(value);
}

/**
  * Suffix operator for floating point literals.
  * 
  * @see Angle<double>::fromHours().
  * 
  * @returns an Angle<double> with the hours literal.
  */
inline Angled operator "" _hours(long double value) {
  return Angled::fromHours(value);
}

/**
  * Suffix operator for floating point literals.
  * 
  * @see Angle<double>::fromCompassPoints().
  * 
  * @returns an Angle<double> with the compass points from the literal.
  */
inline Angled operator "" _compass_points(long double value) {
  return Angled::fromCompassPoints(value);
}

/**
  * Suffix operator for integer literals.
  * 
  * @see Angle<double>::fromHours().
  * 
  * @returns an Angle<double> with the hours from the literal.
  */
inline Angled operator "" _oclock(unsigned long long int value) {
  return Angled::fromClock(value);
}

/**
  * Suffix operator for integer literals.
  * 
  * @see Angle<double>::fromHours().
  * 
  * @returns an Angle<double> with the hours from the literal.
  */
inline Angled operator "" _hours(unsigned long long int value) {
  return Angled::fromHours(value);
}

/**
  * Suffix operator for integer literals.
  * 
  * @see Angle<double>::fromCompassPoints().
  * 
  * @returns an Angle<double> with the compass points from the literal.
  */
inline Angled operator "" _compass_points(unsigned long long int value) {
  return Angled::fromCompassPoints(value);
}
