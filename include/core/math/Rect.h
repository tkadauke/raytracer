#pragma once

class Rect {
public:
  Rect()
    : m_x(0), m_y(0), m_width(0), m_height(0) {}
  Rect(int width, int height)
    : m_x(0), m_y(0), m_width(width), m_height(height) {}
  Rect(int x, int y, int width, int height)
    : m_x(x), m_y(y), m_width(width), m_height(height) {}
  
  inline int x() const { return m_x; }
  inline int y() const { return m_y; }
  inline int width() const { return m_width; }
  inline int height() const { return m_height; }
  
  inline int left() const { return m_x; }
  inline int top() const { return m_y; }
  inline int right() const { return m_x + m_width; }
  inline int bottom() const { return m_y + m_height; }
  
private:
  int m_x, m_y, m_width, m_height;
};
