#pragma once

#include <iostream>

#include "core/DivisionByZeroException.h"
#include "core/InequalityOperator.h"

/**
  * This is a generic color class that represents an RGB color. The type
  * parameter @p T should be a floating-point type, since the color components
  * are values between 0 and 1. Some basic vector operations are defined on this
  * class, like color addition, subtraction, multiplication with a scalar, and
  * another color (component-wise multiplication).
  * 
  * This class supports three color models: RGB, CMYK, and HSV. Internally, the
  * color is stored using the RGB model.
  * 
  * * To use the RGB model, either use the component-wise constructor, or if
  *   you want to create a color from integer values, use the fromRGB() method.
  *   Additionally, there are r(), g() and b() getters (and their long form
  *   aliases) to retrieve the components.
  * * To use the CMYK model, use the fromCMYK() method. The c(), m(), y() and
  *   k() getters (and their long form aliases) return the individual
  *   components.
  * * To use the HSV model, use the fromHSV() method. The h(), s(), and v()
  *   getters (and their long form aliases) return the individual components.
  * 
  * @tparam T the component type, should be a floating point type.
  */
template<class T>
class Color : public InequalityOperator<Color<T>> {
  typedef T ComponentsType[3];

public:
  typedef T Component;
  
  static const Color<T>& black();
  static const Color<T>& white();
  static const Color<T>& red();
  static const Color<T>& green();
  static const Color<T>& blue();
  
  /**
    * Default constructor. Initializes all components with 0, which means the
    * color will be black.
    */
  inline Color() {
    for (int i = 0; i != 3; ++i) {
      m_components[i] = T();
    }
  }

  /**
    * Constructs a color from an array of values of type @p T.
    */
  inline explicit Color(const ComponentsType& cells) {
    for (int i = 0; i != 3; ++i) {
      m_components[i] = cells[i];
    }
  }
  
  /**
    * Constructs a color from the component values @p r, @p g, and @p b.
    */
  inline Color(const T& r, const T& g, const T& b) {
    m_components[0] = r;
    m_components[1] = g;
    m_components[2] = b;
  }

  /**
    * @returns a color that is made up of the integer components @p r, @p b, and
    * @p b. Each of the components is divided by 255.
    */
  inline static Color<T> fromRGB(unsigned int r, unsigned int g, unsigned int b) {
    return Color(T(r) / T(255), T(g) / T(255), T(b) / T(255));
  }
  
  /**
    * Creates a color from CMYK values (Cyan, Magenta, Yellow, and blacK). The
    * conversion works as follows:
    * 
    * * The red component is calculated from the cyan and black colors using
    *   this formula: \f$r = (1-c) \times (1-k)\f$.
    * * The green component is calculated from the magenta and black colors
    *   using this formula: \f$g = (1-m) \times (1-k)\f$.
    * * The blue component is calculated from the yellow and black colors using
    *   this formula: \f$b = (1-y) \times (1-k)\f$.
    * 
    * @see http://www.rapidtables.com/convert/color/cmyk-to-rgb.htm
    */
  inline static Color<T> fromCMYK(const T& c, const T& m, const T& y, const T& k) {
    return Color(
      (T(1) - c) * (T(1) - k),
      (T(1) - m) * (T(1) - k),
      (T(1) - y) * (T(1) - k)
    );
  }
  
  /**
    * Creates a color from HSV values (Hue, Saturation, and Value). The formula
    * to convert from HSV to RGB is fairly complex.
    * 
    * @see http://www.rapidtables.com/convert/color/hsv-to-rgb.htm
    */
  inline static Color<T> fromHSV(unsigned int h, const T& s, const T& v) {
    auto c = v * s;
    auto x = c * (T(1) - std::abs((int(h) / 60) % 2 - 1));
    auto m = v - c;
    
    if (h < 60) {
      return Color(c + m, x + m,     m);
    } else if (60 <= h && h < 120) {
      return Color(x + m, c + m,     m);
    } else if (120 <= h && h < 180) {
      return Color(    m, c + m, x + m);
    } else if (180 <= h && h < 240) {
      return Color(    m, x + m, c + m);
    } else if (240 <= h && h < 300) {
      return Color(x + m,     m, c + m);
    } else {
      return Color(c + m,     m, x + m);
    }
  }

  /**
    * @returns the color component at @p index. This index must be 0, 1, or 2.
    */
  inline const T& component(int index) const {
    return m_components[index];
  }
  
  /**
    * Sets the component at @p index to @p value. The index must be 0, 1, or 2.
    */
  inline void setComponent(int index, const T& value) {
    m_components[index] = value;
  }
  
  /**
    * Non-const index operator, allowing to mutate the color.
    * 
    * @returns a non-const reference to the color component at @p index. This
    *   index must be 0, 1, or 2.
    */
  inline T& operator[](int index) {
    return m_components[index];
  }
  
  /**
    * Const index operator.
    * 
    * @returns a const reference to the color component at @p index. This index
    *   must be 0, 1, or 2.
    */
  inline const T& operator[](int index) const {
    return m_components[index];
  }
  
  /**
    * @returns the internally stored red component as a const reference. The
    *   value is in the range \f$[0, 1]\f$.
    */
  inline const T& r() const {
    return component(0);
  }
  
  /**
    * @returns the internally stored green component as a const reference. The
    *   value is in the range \f$[0, 1]\f$.
    */
  inline const T& g() const {
    return component(1);
  }
  
  /**
    * @returns the internally stored blue component as a const reference. The
    *   value is in the range \f$[0, 1]\f$.
    */
  inline const T& b() const {
    return component(2);
  }
  
  /**
    * @returns this color's black key color from the CMYK color space. This is
    *   calculated from the r(), g(), and b() colors: \f$k = 1-max(r, g, b)\f$.
    */
  inline T k() const {
    return T(1) - max();
  }
  
  /**
    * @returns this color's cyan color from the CMYK color space. This is
    *   calculated from the r() component and the black key color:
    *   \f$c = (1-r-k) / (1-k)\f$, unless \f$1-k = 0\f$, in which case 0 is
    *   returned. This can only happen if the color is fully black.
    */
  inline T c() const {
    auto w = (T(1) - k());
    if (w == 0)
      return 0;
    return (w - r()) / w;
  }
  
  /**
    * @returns this color's magenta color from the CMYK color space. This is
    *   calculated from the g() component and the black key color:
    *   \f$m = (1-g-k) / (1-k)\f$, unless \f$1-k = 0\f$, in which case 0 is
    *   returned. This can only happen if the color is fully black.
    */
  inline T m() const {
    auto w = (T(1) - k());
    if (w == 0)
      return 0;
    return (w - g()) / w;
  }
  
  /**
    * @returns this color's yellow color from the CMYK color space. This is
    *   calculated from the b() component and the black key color:
    *   \f$y = (1-b-k) / (1-k)\f$, unless \f$1-k = 0\f$, in which case 0 is
    *   returned. This can only happen if the color is fully black.
    */
  inline T y() const {
    auto w = (T(1) - k());
    if (w == 0)
      return 0;
    return (w - b()) / w;
  }
  
  /**
    * @returns this color's hue from the HSV color space. The calculation is
    *   fairly complex.
    * 
    * @see http://www.rapidtables.com/convert/color/rgb-to-hsv.htm
    */
  inline unsigned int h() const {
    auto cmax = max();
    auto delta = cmax - min();
    
    int result;
    if (delta == 0) {
      return 0;
    } else if (cmax == r()) {
      result = 60 * ((g() - b()) / delta);
    } else if (cmax == g()) {
      result = 60 * ((b() - r()) / delta + 2);
    } else {
      result = 60 * ((r() - g()) / delta + 4);
    }
    // At this point, result is an angle, which can be negative. Let's be safe
    // and add 720, even though it's unlikely that more than 360 is needed.
    return unsigned(result + 720) % 360;
  }
  
  /**
    * @returns this color's saturation from the HSV color space. The calculation
    *   is fairly complex.
    * 
    * @see http://www.rapidtables.com/convert/color/rgb-to-hsv.htm
    */
  inline T s() const {
    auto cmax = max();
    auto delta = cmax - min();
    
    if (cmax == 0) {
      return 0;
    } else {
      return delta / cmax;
    }
  }
  
  /**
    * @returns this color's value from the HSV color space. This is the simplest
    *   part of the conversion between RGB and HSV: it's just the largest color
    *   component, i.e. v() is an alias for max().
    * 
    * @see http://www.rapidtables.com/convert/color/rgb-to-hsv.htm
    */
  inline T v() const {
    return max();
  }
  
  /**
    * @returns the color component that is the largest.
    */
  inline T max() const {
    return std::max(r(), std::max(g(), b()));
  }
  
  /**
    * @returns the color component that is the smallest.
    */
  inline T min() const {
    return std::min(r(), std::min(g(), b()));
  }
    
  /**
    * @returns a color that is the sum between this color and @p other. The sum
    *   is calculated component-wise.
    */
  inline Color<T> operator+(const Color<T>& other) const {
    Color<T> result;
    for (int i = 0; i != 3; ++i) {
      result.setComponent(i, component(i) + other.component(i));
    }
    return result;
  }

  /**
    * Adds @p other to this color, by adding its components to this color's
    * components.
    */
  inline Color<T>& operator+=(const Color<T>& other) {
    for (int i = 0; i != 3; ++i) {
      setComponent(i, component(i) + other.component(i));
    }
    return *this;
  }

  /**
    * @returns a color that is the difference between this color and @p other.
    *   The difference is calculated component-wise.
    */
  inline Color<T> operator-(const Color<T>& other) const {
    Color<T> result;
    for (int i = 0; i != 3; ++i) {
      result.setComponent(i, component(i) - other.component(i));
    }
    return result;
  }

  /**
    * @returns a color that is this color divided by @p factor, which is a
    *   scalar. This is done by dividing each component by the factor. This
    *   method raises a DivisionByZeroException if @p factor is 0.
    */
  inline Color<T> operator/(const T& factor) const {
    if (factor == T())
      throw DivisionByZeroException(__FILE__, __LINE__);

    T recip = 1.0 / factor;
    return *this * recip;
  }

  /**
    * @returns a color that is this color multiplied by @p factor, which is a
    *   scalar. This is done by multiplying each component by the factor.
    */
  inline Color<T> operator*(const T& factor) const {
    Color<T> result;
    for (int i = 0; i != 3; ++i) {
      result.setComponent(i, component(i) * factor);
    }
    return result;
  }
  
  /**
    * @returns a color that is this color multiplied by @p intensity, which is
    *   another color. This is done by multiplying the colors component-wise.
    */
  inline Color<T> operator*(const Color<T>& intensity) const {
    Color<T> result;
    for (int i = 0; i != 3; ++i) {
      result.setComponent(i, component(i) * intensity.component(i));
    }
    return result;
  }
  
  /**
    * @returns true if this color is equal to @p other. This is determined by
    *   comparing each component.
    */
  inline bool operator==(const Color<T>& other) const {
    for (int i = 0; i != 3; ++i) {
      if (component(i) != other.component(i))
        return false;
    }
    return true;
  }

  /**
    * @returns the red value of this color as an integer, i.e. the internally
    *   stored color multiplied by 255. This value is clipped to \f$[0,255]\f$.
    */
  inline unsigned char rInt() const {
    return std::min(unsigned(r() * 255), 255u);
  }
  
  /**
    * @returns the green value of this color as an integer, i.e. the internally
    *   stored color multiplied by 255. This value is clipped to \f$[0,255]\f$.
    */
  inline unsigned char gInt() const {
    return std::min(unsigned(g() * 255), 255u);
  }
  
  /**
    * @returns the blue value of this color as an integer, i.e. the internally
    *   stored color multiplied by 255. This value is clipped to \f$[0,255]\f$.
    */
  inline unsigned char bInt() const {
    return std::min(unsigned(b() * 255), 255u);
  }
  
  /**
    * @returns the color as an unsigned integer combied RGB value. The result is
    *   a 4 byte long integer, which is has this form: \f$0 r g b\f$.
    */
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

template<class T>
const Color<T>& Color<T>::red() {
  static Color<T> c(1, 0, 0);
  return c;
}

template<class T>
const Color<T>& Color<T>::green() {
  static Color<T> c(0, 1, 0);
  return c;
}

template<class T>
const Color<T>& Color<T>::blue() {
  static Color<T> c(0, 0, 1);
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
