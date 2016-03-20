#pragma once

#include <cmath>
#include "core/math/Polynomial.h"
#include "core/math/Number.h"

template<class T>
class Quadric : public Polynomial<T, 2> {
public:
  typedef Polynomial<T, 2> Base;
  
  inline explicit Quadric(T a, T b, T c)
    : m_a(a), m_b(b), m_c(c)
  {
  }
  
  int solve();

private:
  using Base::m_result;
  T m_a, m_b, m_c;
};

template<class T>
int Quadric<T>::solve() {
  T determinant = m_b * m_b - 4 * m_a * m_c;

  if (isAlmostZero(determinant)) {
    m_result[0] = - m_b / (2 * m_a);
    return 1;
  } else if (determinant > 0) {
    T determinantRoot = std::sqrt(determinant);

    m_result[0] = (-determinantRoot - m_b) / (2 * m_a);
    m_result[1] = (+determinantRoot - m_b) / (2 * m_a);
    return 2;
  } else
    return 0;
}
