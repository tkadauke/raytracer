#ifndef BUFFER_H
#define BUFFER_H

#include "Color.h"
#include "math/Rect.h"

class Buffer {
public:
  typedef Colord* RowType;
  typedef RowType* BufferType;

  inline Buffer(int width, int height)
    : m_width(width), m_height(height)
  {
    m_buffer = new Colord*[height];
    
    for (int i = 0; i != height; ++i)
      m_buffer[i] = new Colord[width];
  }
  
  inline ~Buffer() {
    for (int i = 0; i != m_height; ++i)
      delete [] m_buffer[i];
    
    delete [] m_buffer;
  }
  
  inline const RowType& operator[](int index) const { return m_buffer[index]; }
  inline RowType& operator[](int index) { return m_buffer[index]; }
  
  inline int width() const { return m_width; }
  inline int height() const { return m_height; }
  
  inline Rect rect() const { return Rect(m_width, m_height); }

private:
  BufferType m_buffer;
  int m_width, m_height;
};

#endif
