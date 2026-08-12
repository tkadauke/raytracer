#pragma once

#include <algorithm>
#include <array>
#include <cmath>

/**
  * CRTP mixin that provides Color methods expressible purely in terms of the
  * r(), g(), b() accessors and the three-component constructor.  All three
  * Color variants (generic Color<T>, SSE Color<float>, SSE3 Color<double>)
  * inherit from this class and no longer need to define these methods
  * individually.
  *
  * @tparam Derived  the concrete Color specialisation (e.g. Color<float>).
  * @tparam T        the component scalar type (float or double).
  */
template<class Derived, class T>
class ColorBase {
public:
  /**
    * @returns a color that is made up of the integer components @p r, @p g,
    *   and @p b. Each of the components is divided by 255.
    */
  inline static Derived fromRGB(unsigned int r, unsigned int g, unsigned int b) {
    return Derived(T(r) / T(255), T(g) / T(255), T(b) / T(255));
  }

  /**
    * @returns a color made up of the 24-bit packed RGB value @p rgb, with
    *   red in bits 16-23, green in bits 8-15, and blue in bits 0-7.
    */
  inline static Derived fromPackedRGB(unsigned int rgb) {
    return fromRGB((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
  }

  /**
    * Creates a color from CMYK values (Cyan, Magenta, Yellow, and blacK).
    */
  inline static Derived fromCMYK(const T& c, const T& m, const T& y, const T& k) {
    return Derived((T(1) - c) * (T(1) - k), (T(1) - m) * (T(1) - k), (T(1) - y) * (T(1) - k));
  }

  /**
    * Creates a color from HSV values (Hue, Saturation, and Value).
    */
  inline static Derived fromHSV(unsigned int h, const T& s, const T& v) {
    auto c = v * s;
    auto x = c * (T(1) - std::abs((int(h) / 60) % 2 - 1));
    auto m = v - c;

    if (h < 60) {
      return Derived(c + m, x + m, m);
    } else if (60 <= h && h < 120) {
      return Derived(x + m, c + m, m);
    } else if (120 <= h && h < 180) {
      return Derived(m, c + m, x + m);
    } else if (180 <= h && h < 240) {
      return Derived(m, x + m, c + m);
    } else if (240 <= h && h < 300) {
      return Derived(x + m, m, c + m);
    } else {
      return Derived(c + m, m, x + m);
    }
  }

  /**
    * @returns the RGB components as an array.
    */
  [[nodiscard]] inline std::array<T, 3> toArray() const {
    return {derived().r(), derived().g(), derived().b()};
  }

  /**
    * @returns the RGB components as floats with @p alpha as the fourth value.
    */
  [[nodiscard]] inline std::array<float, 4> toFloat4(float alpha = 1.0f) const {
    return {static_cast<float>(derived().r()), static_cast<float>(derived().g()),
            static_cast<float>(derived().b()), alpha};
  }

  /**
    * @returns the linear interpolation between this color and @p other at
    *   parameter \f$t\f$.
    */
  [[nodiscard]] inline Derived lerp(const Derived& other, const T& t) const {
    return derived() * (T(1) - t) + other * t;
  }

  /**
    * @returns this color's black key color from the CMYK color space.
    */
  inline T k() const {
    return T(1) - max();
  }

  /**
    * @returns this color's cyan color from the CMYK color space.
    */
  inline T c() const {
    auto w = T(1) - k();
    if (w == 0)
      return T(0);
    return (w - derived().r()) / w;
  }

  /**
    * @returns this color's magenta color from the CMYK color space.
    */
  inline T m() const {
    auto w = T(1) - k();
    if (w == 0)
      return T(0);
    return (w - derived().g()) / w;
  }

  /**
    * @returns this color's yellow color from the CMYK color space.
    */
  inline T y() const {
    auto w = T(1) - k();
    if (w == 0)
      return T(0);
    return (w - derived().b()) / w;
  }

  /**
    * @returns this color's hue from the HSV color space.
    */
  inline unsigned int h() const {
    auto cmax = max();
    auto delta = cmax - min();

    int result;
    if (delta == 0) {
      return 0;
    } else if (cmax == derived().r()) {
      result = 60 * ((derived().g() - derived().b()) / delta);
    } else if (cmax == derived().g()) {
      result = 60 * ((derived().b() - derived().r()) / delta + 2);
    } else {
      result = 60 * ((derived().r() - derived().g()) / delta + 4);
    }
    return unsigned(result + 720) % 360;
  }

  /**
    * @returns this color's saturation from the HSV color space.
    */
  inline T s() const {
    auto cmax = max();
    auto delta = cmax - min();

    if (cmax == 0) {
      return T(0);
    } else {
      return delta / cmax;
    }
  }

  /**
    * @returns this color's value from the HSV color space (the largest
    *   component).
    */
  inline T v() const {
    return max();
  }

  /**
    * @returns the color component that is the largest.
    */
  inline T max() const {
    return std::max(derived().r(), std::max(derived().g(), derived().b()));
  }

  /**
    * @returns the color component that is the smallest.
    */
  inline T min() const {
    return std::min(derived().r(), std::min(derived().g(), derived().b()));
  }

  /**
    * @returns the Rec. 601 luminance for this color.
    */
  inline T luminance() const {
    return derived().r() * T(0.299) + derived().g() * T(0.587) + derived().b() * T(0.114);
  }

  /**
    * @returns a color containing each component squared.
    */
  inline Derived squared() const {
    return derived() * derived();
  }

  /**
    * @returns the sum of the squared color components.
    */
  inline T squaredMagnitude() const {
    const Derived sq = derived().squared();
    return sq.r() + sq.g() + sq.b();
  }

  /**
    * @returns the red value of this color as an integer, clipped to [0, 255].
    */
  inline unsigned char rInt() const {
    return std::min(unsigned(derived().r() * 255), 255u);
  }

  /**
    * @returns the green value of this color as an integer, clipped to [0, 255].
    */
  inline unsigned char gInt() const {
    return std::min(unsigned(derived().g() * 255), 255u);
  }

  /**
    * @returns the blue value of this color as an integer, clipped to [0, 255].
    */
  inline unsigned char bInt() const {
    return std::min(unsigned(derived().b() * 255), 255u);
  }

  /**
    * @returns the color as an unsigned integer packed RGB value of the form
    *   0xRRGGBB.
    */
  inline unsigned int rgb() const {
    return rInt() << 16 | gInt() << 8 | bInt();
  }

  /**
    * @returns true if this color is equal to @p other. This is determined by
    *   comparing each component.
    */
  inline bool operator==(const Derived& other) const {
    for (int i = 0; i != 3; ++i) {
      if (derived().component(i) != other.component(i))
        return false;
    }
    return true;
  }

private:
  inline const Derived& derived() const {
    return static_cast<const Derived&>(*this);
  }
};
