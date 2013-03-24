#ifndef QUARTIC_H
#define QUARTIC_H

#include <cmath>
#include "math/Polynomial.h"
#include "math/Number.h"
#include "math/Cubic.h"
#include "math/Quadric.h"

template<class T>
class Quartic : public Polynomial<T, 4> {
public:
  typedef Polynomial<T, 4> normBase;
  
  Quartic(T a, T b, T c, T d, T e)
    : m_a(a), m_b(b), m_c(c), m_d(d), m_e(e)
  {
  }
  
  int solve();

private:
  using normBase::m_result;
  T m_a, m_b, m_c, m_d, m_e;
};

template<class T>
int Quartic<T>::solve() {
  T normA = m_b / m_a;
  T normB = m_c / m_a;
  T normC = m_d / m_a;
  T normD = m_e / m_a;
  
  T normASquared = normA * normA;
  T p = -3.0/8 * normASquared + normB;
  T q = 1.0/8 * normASquared * normA - 0.5 * normA * normB + normC;
  T r = -3.0/256 * normASquared * normASquared + 1.0/16 * normASquared * normB - 0.25 * normA * normC + normD;
  
  int numberOfResults;
  if (isAlmostZero(r, T(0.000001))) {
    Cubic<T> cubic(1, 0, p, q);
    numberOfResults = cubic.solveInto(m_result);
  } else {
    Cubic<T> cubic(1, -0.5 * p, -r, 0.5 * r * p - 1.0/8 * q * q);
    cubic.solveInto(m_result);
  
    T z = m_result[0];
  
    T u = z * z - r;
    T v = 2 * z - p;
  
    if (isAlmostZero(u, T(0.000001)))
      u = 0;
    else if (u > 0)
      u = std::sqrt(u);
    else
      return 0;
  
    if (isAlmostZero(v, T(0.000001)))
      v = 0;
    else if (v > 0)
      v = std::sqrt(v);
    else
      return 0;
  
    Quadric<T> first(1, q < 0 ? -v : v, z - u);
    numberOfResults = first.solveInto(m_result);
  
    Quadric<T> second(1, q < 0 ? v : -v, z + u);
    numberOfResults += second.solveInto(m_result + numberOfResults);
  }
  
  T sub = 0.25 * normA;
  
  for (int i = 0; i < numberOfResults; ++i)
    m_result[i] -= sub;
  
  return numberOfResults;
}

#endif
