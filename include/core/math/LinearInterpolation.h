#pragma once

#include "core/InequalityOperator.h"

/**
  * Represents a linear interpolation between a begin and end of a range. It
  * supports iterating over an interval linearly in a given number of steps.
  */
template<class T>
class LinearInterpolation {
public:
  /**
    * Iterator class to iterate over the range specified by the linear
    * interpolation.
    * 
    * This class inherits from InequalityOperator, which provides the !=
    * operator.
    */
  class Iterator : public InequalityOperator<Iterator> {
  public:
    /**
      * @returns the current step number.
      */
    inline int current() const {
      return m_current;
    }
    
    /**
      * Advances this iterator, modyfying it inplace.
      * 
      * @returns the advanced iterator.
      */
    inline Iterator& operator++() {
      m_current++;
      return *this;
    }
    
    /**
      * Advances this iterator, modifying it inplace. This method will create
      * a copy of the iterator which is returned. If you don't need that copy,
      * please use the prefix ++ operator instead.
      * 
      * @returns the original unadvanced iterator.
      */
    inline Iterator operator++(int) {
      Iterator tmp(*this);
      m_current++;
      return tmp;
    }
    
    /**
      * @returns true if this iterator is equal to @p other, false otherwise. To
      *   determine equality, the current step number as well as the begin of
      *   the interval and the step size are compared.
      */
    inline bool operator==(const Iterator& other) const {
      return m_begin == other.m_begin && m_step == other.m_step && m_current == other.m_current;
    }
    
    /**
      * @returns the current value of the linear interpolation.
      */
    inline T operator*() const {
      return m_begin + m_step * m_current;
    }
  
  private:
    friend class LinearInterpolation;
    
    inline explicit Iterator(const T& begin, const T& end, int steps, int current = 0)
      : m_begin(begin),
        m_current(current)
    {
      m_step = (end - begin) / steps;
    }
    
    T m_begin, m_step;
    int m_current;
  };
  
  /**
    * Constructor. Creates a linear interpolation between @p begin and @p end,
    * using the specified number of @p steps.
    */
  inline explicit LinearInterpolation(const T& begin, const T& end, int steps)
    : m_begin(begin),
      m_end(end),
      m_steps(steps)
  {
  }
  
  /**
    * @returns an iterator pointing to the beginning of the interval.
    */
  inline Iterator begin() const {
    return Iterator(m_begin, m_end, m_steps, 0);
  }
  
  /**
    * @returns an iterator pointing past the end of the interval.
    */
  inline Iterator end() const {
    return Iterator(m_begin, m_end, m_steps, m_steps);
  }
  
private:
  T m_begin, m_end;
  int m_steps;
};
