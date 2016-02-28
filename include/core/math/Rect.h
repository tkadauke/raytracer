#pragma once

#include <iostream>

/**
  * Represents a two-dimensional rectangle with integer coordinates.
  */
class Rect {
public:
  /**
    * Constructs a null rectangle.
    */
  inline Rect()
    : m_x(0),
      m_y(0),
      m_width(0),
      m_height(0)
  {
  }
  
  /**
    * Constructs a rectangle originating at \f$(0, 0)\f$ with the given width
    * and height.
    */
  inline Rect(int width, int height)
    : m_x(0),
      m_y(0),
      m_width(width),
      m_height(height)
  {
  }
  
  /**
    * Constructs a rectangle originating at \f$(x, y)\f$ with the given width
    * and height.
    */
  inline Rect(int x, int y, int width, int height)
    : m_x(x),
      m_y(y),
      m_width(width),
      m_height(height)
  {
  }
  
  /**
    * Returns the left edge of the rectangle.
    */
  inline int x() const {
    return m_x;
  }
  
  /**
    * Returns the top edge of the rectangle.
    */
  inline int y() const {
    return m_y;
  }
  
  /**
    * Returns the width of the rectangle.
    */
  inline int width() const {
    return m_width;
  }
  
  /**
    * Returns the height of the rectangle.
    */
  inline int height() const {
    return m_height;
  }
  
  /**
    * Returns the left edge of the rectangle.
    */
  inline int left() const {
    return m_x;
  }
  
  /**
    * Returns the top edge of the rectangle.
    */
  inline int top() const {
    return m_y;
  }
  
  /**
    * Returns the right edge of the rectangle.
    */
  inline int right() const {
    return m_x + m_width;
  }
  
  /**
    * Returns the bottom edge of the rectangle.
    */
  inline int bottom() const {
    return m_y + m_height;
  }
  
private:
  int m_x, m_y, m_width, m_height;
};

/**
  * Serializes rect to the given std::ostream.
  * 
  * @returns os.
  */
inline std::ostream& operator<<(std::ostream& os, const Rect& rect) {
  os << "Rect("
     << rect.left() << ", " << rect.top() << " to "
     << rect.right() << ", " << rect.bottom()
     << ")";
  return os;
}
