#pragma once

#include "core/InequalityOperator.h"

template<class T>
class LinearInterpolation {
public:
  class Iterator : public InequalityOperator<Iterator> {
  public:
    inline explicit Iterator(const T& begin, const T& end, int steps, int current = 0)
      : m_begin(begin), m_current(current)
    {
      m_step = (end - begin) / steps;
    }
    
    inline int current() const {
      return m_current;
    }
    
    inline Iterator& operator++() {
      m_current++;
      return *this;
    }
    
    inline Iterator operator++(int) {
      Iterator tmp(*this);
      m_current++;
      return tmp;
    }
    
    inline bool operator==(const Iterator& other) const {
      // TODO: raise exception if iterators are incompatible?
      return m_current == other.m_current;
    }
    
    inline T operator*() const {
      return m_begin + m_step * m_current;
    }
  
  private:
    T m_begin, m_step;
    int m_current;
  };
  
  inline explicit LinearInterpolation(const T& begin, const T& end, int steps)
    : m_begin(begin), m_end(end), m_steps(steps)
  {
  }
  
  inline Iterator begin() const {
    return Iterator(m_begin, m_end, m_steps, 0);
  }
  
  inline Iterator end() const {
    return Iterator(m_begin, m_end, m_steps, m_steps);
  }
  
private:
  T m_begin, m_end;
  int m_steps;
};
