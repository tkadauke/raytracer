#pragma once

// C++20 migration note: replace these inline constexpr definitions with
// <numbers> (std::numbers::pi, std::numbers::e, std::numbers::sqrt2, etc.)
// and derived aliases once the project adopts C++20 (roadmap §3.1).

/**
  * Approximation of \f$\pi\f$, see [Pi](https://en.wikipedia.org/wiki/Pi).
  */
inline constexpr double PI = 3.1415926535897932384;

/**
  * Approximation of \f$\tau = 2\pi\f$, see
  * <a href="https://en.wikipedia.org/wiki/Turn_(geometry)#Tau_proposal">Tau</a>.
  */
inline constexpr double TAU = 6.2831853071795864769;

/**
  * Approximation of \f$\frac{1}{\pi}\f$.
  */
inline constexpr double invPI = 0.3183098861837906715;

/**
  * Approximation of \f$\frac{1}{\tau}\f$.
  */
inline constexpr double invTAU = 0.1591549430918953358;

/**
  * Approximation of \f$\frac{\pi}{2}\f$.
  */
inline constexpr double PI_OVER_2 = 1.5707963267948966192;

/**
  * Approximation of \f$\frac{\pi}{4}\f$.
  */
inline constexpr double PI_OVER_4 = 0.7853981633974483096;

/**
  * Conversion factor from degrees to radians, \f$\frac{\pi}{180}\f$.
  */
inline constexpr double DEG_TO_RAD = 0.0174532925199432957;

/**
  * Conversion factor from radians to degrees, \f$\frac{180}{\pi}\f$.
  */
inline constexpr double RAD_TO_DEG = 57.2957795130823229;

/**
  * Approximation of \f$\sqrt{2}\f$.
  */
inline constexpr double SQRT2 = 1.4142135623730950488;

/**
  * Approximation of \f$\sqrt{3}\f$.
  */
inline constexpr double SQRT3 = 1.7320508075688772935;

/**
  * Approximation of Euler's number \f$e\f$,
  * see [E](https://en.wikipedia.org/wiki/E_(mathematical_constant)).
  */
inline constexpr double E = 2.7182818284590452353;

/**
  * Approximation of the golden ratio \f$\varphi = \frac{1+\sqrt{5}}{2}\f$,
  * see [Golden ratio](https://en.wikipedia.org/wiki/Golden_ratio).
  */
inline constexpr double GOLDEN_RATIO = 1.6180339887498948482;
