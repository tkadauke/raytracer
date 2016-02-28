#pragma once

#include "core/math/Rect.h"

template<class T>
class Buffer {
public:
  typedef T ElementType;
  typedef T* RowType;
  typedef RowType* BufferType;

  inline Buffer(int width, int height)
    : m_width(width),
      m_height(height)
  {
    m_buffer = new T*[height];
    
    for (int i = 0; i != height; ++i)
      m_buffer[i] = new T[width];
  }
  
  inline ~Buffer() {
    for (int i = 0; i != m_height; ++i)
      delete [] m_buffer[i];
    
    delete [] m_buffer;
  }
  
  inline const RowType& operator[](int index) const {
    return m_buffer[index];
  }
  
  inline RowType& operator[](int index) {
    return m_buffer[index];
  }
  
  inline int width() const {
    return m_width;
  }
  
  inline int height() const {
    return m_height;
  }
  
  inline Rect rect() const {
    return Rect(m_width, m_height);
  }
  
  void clear() {
    for (int i = 0; i != width(); ++i) {
      for (int j = 0; j != height(); ++j) {
        m_buffer[j][i] = T();
      }
    }
  }

private:
  BufferType m_buffer;
  int m_width, m_height;
};
