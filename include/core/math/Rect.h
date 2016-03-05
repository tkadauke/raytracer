#pragma once

#include <iostream>

/**
  * Represents a two-dimensional rectangle with integer coordinates.
  *
  * @tparam T The coordinate type. Useful types here are int, float, and double,
  *   among others.
  */
template<class T>
class Rect {
public:
  /**
    * Constructs a null rectangle.
    */
  inline Rect()
    : m_x(T()),
      m_y(T()),
      m_width(T()),
      m_height(T())
  {
  }
  
  /**
    * Constructs a rectangle originating at \f$(0, 0)\f$ with the given width
    * and height.
    */
  inline Rect(const T& width, const T& height)
    : m_x(T()),
      m_y(T()),
      m_width(width),
      m_height(height)
  {
  }
  
  /**
    * Constructs a rectangle originating at \f$(x, y)\f$ with the given width
    * and height.
    */
  inline Rect(const T& x, const T& y, const T& width, const T& height)
    : m_x(x),
      m_y(y),
      m_width(width),
      m_height(height)
  {
  }
  
  /**
    * Returns the left edge of the rectangle.
    */
  inline const T& x() const {
    return m_x;
  }
  
  /**
    * Returns the top edge of the rectangle.
    */
  inline const T& y() const {
    return m_y;
  }
  
  /**
    * Returns the width of the rectangle.
    */
  inline const T& width() const {
    return m_width;
  }
  
  /**
    * Returns the height of the rectangle.
    */
  inline const T& height() const {
    return m_height;
  }
  
  /**
    * Returns the left edge of the rectangle.
    */
  inline const T& left() const {
    return m_x;
  }
  
  /**
    * Returns the top edge of the rectangle.
    */
  inline const T& top() const {
    return m_y;
  }
  
  /**
    * Returns the right edge of the rectangle.
    */
  inline T right() const {
    return m_x + m_width;
  }
  
  /**
    * Returns the bottom edge of the rectangle.
    */
  inline T bottom() const {
    return m_y + m_height;
  }
  
private:
  T m_x, m_y, m_width, m_height;
};

/**
  * Serializes rect to the given std::ostream.
  * 
  * @returns os.
  */
template<class T>
inline std::ostream& operator<<(std::ostream& os, const Rect<T>& rect) {
  os << "Rect("
     << rect.left() << ", " << rect.top() << " to "
     << rect.right() << ", " << rect.bottom()
     << ")";
  return os;
}

/**
  * Shortcut for rect of ints.
  */
typedef Rect<int> Recti;

/**
  * Shortcut for rect of floats.
  */
typedef Rect<float> Rectf;

/**
  * Shortcut for rect of doubles.
  */
typedef Rect<double> Rectd;
