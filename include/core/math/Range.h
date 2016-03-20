#pragma once

#include <iostream>

#include "core/math/Number.h"

/**
  * Represents a closed range, between begin() and end(), including.
  * 
  * @tparam T the number type used to store the end points of the range.
  */
template<class T>
class Range {
public:
  /**
    * Creates a new range between begin and end.
    */
  Range(const T& begin, const T& end)
    : m_begin(begin),
      m_end(end)
  {
  }

  /**
    * @returns the begin of the Range.
    */
  inline T begin() const {
    return m_begin;
  }
  
  /**
    * @returns the end of the Range.
    */
  inline T end() const {
    return m_end;
  }

  /**
    * @returns true if value is inside the Range, false otherwise.
    */
  inline bool contains(const T& value) const {
    return begin() <= value && value <= end();
  }
  
  /**
    * @returns value, clamped to the boundaries of this Range.
    */
  inline T clamp(const T& value) const {
    if (value < begin()) {
      return begin();
    } else if (value > end()) {
      return end();
    } else {
      return value;
    }
  }
  
  /**
    * @returns a random number from within the Range.
    */
  inline T random() const {
    return ::random<T>(begin(), end());
  }

private:
  T m_begin;
  T m_end;
};

/**
  * Outputs the range to the given std::ostream.
  * 
  * @returns os.
  */
template<class T>
std::ostream& operator<<(std::ostream& os, const Range<T>& range) {
  os << '['
     << range.begin() << ", " << range.end()
     << ']';

  return os;
}

/**
  * Shortcut for range of floats.
  */
typedef Range<float> Rangef;

/**
  * Shortcut for Range of doubles.
  */
typedef Range<double> Ranged;
