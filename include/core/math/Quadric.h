#pragma once

#include <cmath>
#include "core/math/Polynomial.h"
#include "core/math/Number.h"

/**
  * Represents a quadric polynomial of the form \f$ax^2 + bx + c\f$.
  */
template<class T>
class Quadric : public Polynomial<T, 2> {
public:
  typedef Polynomial<T, 2> Base;
  
  /**
    * Constructor. Takes the @p a, @p b, and @p c coefficients of the
    * polynomial \f$ax^2 + bx + c\f$.
    */
  inline explicit Quadric(T a, T b, T c)
    : m_a(a), m_b(b), m_c(c)
  {
  }
  
  /**
    * Solves the polynomial equation \f$ax^2 + bx + c = 0\f$
    * 
    * @returns the number of solutions.
    * 
    * @see the Polynomial class for information how to retrieve the results.
    */
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
