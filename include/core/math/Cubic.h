#pragma once

#include <cmath>
#include "core/math/Polynomial.h"
#include "core/math/Number.h"

/**
  * Represents a cubic polynomial of the form \f$ax^3 + bx^2 + cx + d\f$.
  */
template<class T>
class Cubic : public Polynomial<T, 3> {
public:
  typedef Polynomial<T, 3> Base;
  
  /**
    * Constructor. Takes the @p a, @p b, @p c, and @p d coefficients of the
    * polynomial \f$ax^3 + bx^2 + cx + d\f$.
    */
  inline explicit Cubic(T a, T b, T c, T d)
    : m_a(a), m_b(b), m_c(c), m_d(d)
  {
  }
  
  /**
    * Solves the polynomial equation \f$ax^3 + bx^2 + cx + d = 0\f$
    * 
    * @returns the number of solutions.
    * 
    * @see the Polynomial class for information how to retrieve the results.
    */
  int solve() override;

private:
  using Base::m_result;
  T m_a, m_b, m_c, m_d;
};

template<class T>
int Cubic<T>::solve() {
  T normA = m_b / m_a;
  T normB = m_c / m_a;
  T normC = m_d / m_a;

  T normASquared = normA * normA;
  T p = 1.0/3 * (- 1.0/3 * normASquared + normB);
  T q = 1.0/2 * (2.0/27 * normA * normASquared - 1.0/3 * normA * normB + normC);

  T pCube = p * p * p;
  T determinant = q * q + pCube;

  int numberOfResults;
  if (isAlmostZero(determinant)) {
    if (isAlmostZero(q)) {
      m_result[0] = 0;
      numberOfResults = 1;
    } else {
      T cubeRoot = std::cbrt(-q);
      m_result[0] = 2 * cubeRoot;
      m_result[1] = - cubeRoot;
      numberOfResults = 2;
    }
  } else if (determinant < 0) {
    T phi = 1.0/3 * std::acos(-q / std::sqrt(-pCube));
    T t = 2 * std::sqrt(-p);

    m_result[0] =  t * std::cos(phi);
    m_result[1] = -t * std::cos(phi + M_PI / 3);
    m_result[2] = -t * std::cos(phi - M_PI / 3);
    numberOfResults = 3;
  } else {
    T determinantRoot = std::sqrt(determinant);
    T u = std::cbrt(determinantRoot - q);
    T v = - std::cbrt(determinantRoot + q);

    m_result[0] = u + v;
    numberOfResults = 1;
  }

  T sub = 1.0/3 * normA;

  for (int i = 0; i < numberOfResults; ++i)
    m_result[i] -= sub;

  return numberOfResults;
}
