#ifndef QUADRIC_H
#define QUADRIC_H

#include <limits>
#include <cmath>

template<class T>
class Quadric {
public:
  typedef T Coefficient;
  typedef T Result[3];
  
  Quadric(T a, T b, T c)
    : m_a(a), m_b(b), m_c(c)
  {
    m_result[0] = std::numeric_limits<T>::quiet_NaN();
    m_result[1] = std::numeric_limits<T>::quiet_NaN();
    m_result[2] = std::numeric_limits<T>::quiet_NaN();
  }
  
  int solve();
  inline const Result& result() const { return m_result; }

private:
  T m_a, m_b, m_c;
  Result m_result;
};

template<class T>
int Quadric<T>::solve() {
  static const T epsilon = std::numeric_limits<T>::epsilon();
  
  T determinant = m_b * m_b - 4 * m_a * m_c;

  if (-epsilon < determinant && determinant < epsilon) {
    m_result[0] = - m_b / (2 * m_a);
    return 1;
  } else if (determinant > 0) {
    T determinantRoot = std::sqrt(determinant);

    m_result[0] = (determinantRoot - m_b) / (2 * m_a);
    m_result[1] = (-determinantRoot - m_b) / (2 * m_a);
    return 2;
  } else
    return 0;
}

#endif
