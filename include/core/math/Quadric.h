#pragma once

#include <cmath>
#include "core/math/Polynomial.h"
#include "core/math/Number.h"

/**
  * Represents a quadric polynomial of the form \f$ax^2 + bx + c\f$.
  */
template<class T>
class Quadric : public Polynomial<T, 2, Quadric<T>> {
public:
  typedef Polynomial<T, 2, Quadric<T>> Base;
  
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
  // Degenerate to a linear equation when the leading coefficient vanishes.
  // Without this branch the ax^2 == 0 case divides by zero in the canonical
  // -b / (2a) formulas — for example, OpenCylinder::intersect with a ray
  // parallel to the cylinder's axis hits this with a = b = 0 and c = -r^2,
  // and the platform-specific NaN handling that follows used to leak a
  // bogus "hit" through the y-range check on x86.
  if (isAlmostZero(m_a)) {
    if (isAlmostZero(m_b)) {
      // 0*x + 0 == 0 has infinitely many solutions; 0*x + c (c != 0) has
      // none. We don't surface "infinite solutions" through this API — any
      // caller that hits c == 0 here is either degenerate or already past
      // the geometric question they were asking, so report no roots.
      return 0;
    }
    m_result[0] = -m_c / m_b;
    return 1;
  }

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
