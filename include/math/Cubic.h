#ifndef CUBIC_H
#define CUBIC_H

#include <limits>
#include <cmath>

template<class T>
class Cubic {
public:
  typedef T Coefficient;
  typedef T Result[3];
  
  Cubic(T a, T b, T c, T d)
    : m_a(a), m_b(b), m_c(c), m_d(d)
  {
    m_result[0] = std::numeric_limits<T>::quiet_NaN();
    m_result[1] = std::numeric_limits<T>::quiet_NaN();
    m_result[2] = std::numeric_limits<T>::quiet_NaN();
  }
  
  int solve();
  inline const Result& result() const { return m_result; }

private:
  T m_a, m_b, m_c, m_d;
  Result m_result;
};

template<class T>
int Cubic<T>::solve() {
  static const T epsilon = std::numeric_limits<T>::epsilon();

  T normA = m_b / m_a;
  T normB = m_c / m_a;
  T normC = m_d / m_a;

  T normASquared = normA * normA;
  T p = 1.0/3 * (- 1.0/3 * normASquared + normB);
  T q = 1.0/2 * (2.0/27 * normA * normASquared - 1.0/3 * normA * normB + normC);

  T pCube = p * p * p;
  T determinant = q * q + pCube;

  int numberOfResults;
  if (-epsilon < determinant && determinant < epsilon) {
    if (-epsilon < q && q < epsilon) {
      m_result[0] = 0;
      numberOfResults = 1;
    } else {
      T cubeRoot = cbrt(-q);
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
    T u = cbrt(determinantRoot - q);
    T v = - cbrt(determinantRoot + q);

    m_result[0] = u + v;
    numberOfResults = 1;
  }

  T sub = 1.0/3 * normA;

  for (int i = 0; i < numberOfResults; ++i)
    m_result[i] -= sub;

  return numberOfResults;
}

#endif
