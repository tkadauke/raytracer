#pragma once

#include "core/math/Rect.h"

/**
  * Represents a two dimensional buffer with value type @p T. The width and
  * height of the buffer are set during construction time.
  * 
  * @tparam T value type for the buffer.
  */
template<class T>
class Buffer {
public:
  typedef T ElementType;
  typedef T* RowType;
  typedef RowType* BufferType;

  /**
    * Constructs a buffer of dimensions @p width and @p height. The constructor
    * does not initialize the buffer, so all values in it are undefined.
    * 
    * @see clear().
    */
  inline explicit Buffer(int width, int height)
      : m_width(width),
        m_height(height) {
    m_buffer = new T*[height];

    for (int i = 0; i != height; ++i)
      m_buffer[i] = new T[width];
  }

  /**
    * Destructor. Deletes the buffer.
    */
  inline ~Buffer() {
    for (int i = 0; i != m_height; ++i)
      delete[] m_buffer[i];

    delete[] m_buffer;
  }

  Buffer(const Buffer&) = delete;

  /**
    * @returns a const-reference to the buffer row at @p index.
    */
  inline const RowType& operator[](int index) const {
    return m_buffer[index];
  }

  /**
    * @returns a mutable reference to the buffer row at index.
    */
  inline RowType& operator[](int index) {
    return m_buffer[index];
  }

  /**
    * @returns the buffer's width.
    */
  inline int width() const {
    return m_width;
  }

  /**
    * @returns the buffer's height.
    */
  inline int height() const {
    return m_height;
  }

  /**
    * @returns an int Rect startubg at the origin with the buffer's dimensions.
    */
  inline Recti rect() const {
    return Recti(m_width, m_height);
  }

  /**
    * Clears the buffer by setting all the values to the default value of @p T.
    */
  inline void clear() {
    clear(T());
  }

  /**
    * Clears the buffer by setting all the values to @p value.
    */
  inline void clear(const T& value) {
    for (int i = 0; i != width(); ++i) {
      for (int j = 0; j != height(); ++j) {
        m_buffer[j][i] = value;
      }
    }
  }

  /**
    * @returns the number of element positions where @p differs reports the
    * values from this buffer and @p other as different. Returns the total
    * element count when the buffers have mismatched dimensions.
    *
    * @tparam Predicate `bool(const T&, const T&)`. For multi-channel value
    * types like `Colord`, callers supply the comparison semantics (per-channel
    * tolerance, mask, etc.); for arithmetic `T` callers usually pass a small
    * tolerance check.
    */
  template<typename Predicate>
  inline int countDifferences(const Buffer& other, Predicate differs) const {
    if (other.width() != width() || other.height() != height()) {
      return width() * height();
    }
    int count = 0;
    for (int y = 0; y != height(); ++y) {
      for (int x = 0; x != width(); ++x) {
        if (differs(m_buffer[y][x], other.m_buffer[y][x])) {
          ++count;
        }
      }
    }
    return count;
  }

private:
  BufferType m_buffer;
  int m_width, m_height;
};
