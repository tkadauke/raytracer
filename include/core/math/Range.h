#pragma once

#include <iostream>

/**
  * Represents a closed range, between begin() and end(), including.
  */
template<class T>
class Range {
public:
  /**
    * Creates a new range between begin and end.
    */
  Range(const T& begin, const T& end)
    : m_begin(begin), m_end(end)
  {
  }

  /**
    * Returns the begin of the Range.
    */
  inline T begin() const { return m_begin; }
  
  /**
    * Returns the end of the Range.
    */
  inline T end() const { return m_end; }

  /**
    * Tests whether value is inside the Range.
    */
  inline bool contains(const T& value) const {
    return begin() <= value && value <= end();
  }
  
  /**
    * Clamps the given value to the boundaries of this Range.
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

private:
  T m_begin;
  T m_end;
};

/**
  * Outputs the range to the given std::ostream.
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
