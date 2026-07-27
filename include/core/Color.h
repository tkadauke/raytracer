#pragma once

#include <algorithm>
#include <array>
#include <iostream>
#include <type_traits>

#include "core/DivisionByZeroException.h"
#include "core/InequalityOperator.h"
#include "core/color/ColorBase.h"

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
  * @htmlonly
  * <script type="text/javascript" src="figure.js"></script>
  * <script type="text/javascript" src="color_model_conversions.js"></script>
  * @endhtmlonly
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
class Color : public ColorBase<Color<T>, T>, public InequalityOperator<Color<T>> {
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
    * Constructs a color from the first three values in the given C array.
    */
  template<class Source, std::size_t Size,
           typename = std::enable_if_t<(Size >= 3u) && std::is_convertible_v<Source, T>>>
  inline explicit Color(const Source (&cells)[Size]) {
    for (int i = 0; i != 3; ++i) {
      m_components[i] = static_cast<T>(cells[static_cast<std::size_t>(i)]);
    }
  }

  /**
    * Constructs a color from the first three values in the given array.
    */
  template<class Source, std::size_t Size,
           typename = std::enable_if_t<(Size >= 3u) && std::is_convertible_v<Source, T>>>
  inline explicit Color(const std::array<Source, Size>& cells) {
    for (int i = 0; i != 3; ++i) {
      m_components[i] = static_cast<T>(cells[static_cast<std::size_t>(i)]);
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
