#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include "core/math/Polynomial.h"
#include "core/math/Number.h"
#include "core/math/Cubic.h"
#include "core/math/Quadric.h"
#include "core/math/StablePolynomial.h"

/**
  * Represents a quartic polynomial of the form
  * \f$ax^4 + bx^3 + cx^2 + dx + e\f$.
  */
template<class T>
class Quartic : public Polynomial<T, 4, Quartic<T>> {
public:
  typedef Polynomial<T, 4, Quartic<T>> Base;

  /**
    * Constructor. Takes the @p a, @p b, @p c, @p d, and @p e coefficients of
    * the polynomial \f$ax^4 + bx^3 + cx^2 + dx + e\f$.
    */
  inline explicit Quartic(T a, T b, T c, T d, T e)
    : m_a(a), m_b(b), m_c(c), m_d(d), m_e(e)
  {
  }

  /**
    * Solves the polynomial equation \f$ax^4 + bx^3 + cx^2 + dx + e = 0\f$
    *
    * @returns the number of solutions.
    *
    * @see the Polynomial class for information how to retrieve the results.
    */
  int solve();

  /**
    * Solves the polynomial using a real-axis isolation fallback. This path is
    * slower than Ferrari's method but preserves root counts for badly scaled
    * quartics such as grazing torus intersections.
    */
  int solveStable();

  /**
    * @returns the sorted real roots from solveStable().
    */
  inline SortedResult<T, 4> stableSortedResult() {
    SortedResult<T, 4> res;
    int num = solveStable();
    for (int i = 0; i != num; ++i) {
      res.values[res.count++] = m_result[i];
    }
    return res;
  }

  /**
    * @returns true when the coefficient magnitudes are spread enough that the
    * closed-form path is likely to lose precision.
    */
  bool shouldUseStableSolver() const;

private:
  using Base::m_result;
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

  int numberOfResults = 0;
  if (isAlmostZero(r)) {
    Cubic<T> cubic(1, 0, p, q);
    numberOfResults = cubic.solveInto(m_result);
  } else {
    Cubic<T> cubic(1, -0.5 * p, -r, 0.5 * r * p - 1.0/8 * q * q);
    cubic.solveInto(m_result);

    T z = m_result[0];

    T u = z * z - r;
    T v = 2 * z - p;

    // A real root z of the resolvent cubic mathematically satisfies u >= 0 and
    // v >= 0. Finite-precision arithmetic can place either just below zero by
    // O(eps) relative to the input scale; the absolute epsilon of
    // isAlmostZero (~2.22e-15 for double) is too tight to absorb that — for
    // example, the quartic (1,-16,86,-176,105) gives z = 2.999...9 and
    // u = -2.66e-15, which would otherwise return 0 solutions instead of 4.
    // Use a tolerance scaled to the magnitude of the inputs.
    const T eps = std::numeric_limits<T>::epsilon() * 16;
    T uTol = eps * (T(1) + std::abs(z * z) + std::abs(r));
    T vTol = eps * (T(1) + std::abs(2 * z) + std::abs(p));

    if (u < -uTol) return 0;
    u = u <= T(0) ? T(0) : std::sqrt(u);

    if (v < -vTol) return 0;
    v = v <= T(0) ? T(0) : std::sqrt(v);

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

template<class T>
int Quartic<T>::solveStable() {
  auto stableRoots = StablePolynomial<T>::solveQuartic(m_a, m_b, m_c, m_d, m_e);
  for (std::size_t i = 0; i < stableRoots.count; ++i) {
    m_result[i] = stableRoots.values[i];
  }
  return static_cast<int>(stableRoots.count);
}

template<class T>
bool Quartic<T>::shouldUseStableSolver() const {
  T maxCoefficient = std::max({std::abs(m_a), std::abs(m_b), std::abs(m_c),
                               std::abs(m_d), std::abs(m_e)});
  if (maxCoefficient == T(0) ||
      std::abs(m_a) <= std::numeric_limits<T>::epsilon() * maxCoefficient) {
    return true;
  }

  T minNonZeroCoefficient = maxCoefficient;
  for (T coefficient : {m_a, m_b, m_c, m_d, m_e}) {
    T magnitude = std::abs(coefficient);
    if (magnitude > T(0)) {
      minNonZeroCoefficient = std::min(minNonZeroCoefficient, magnitude);
    }
  }

  return maxCoefficient / minNonZeroCoefficient > T(100000);
}
