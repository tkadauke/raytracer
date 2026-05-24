#pragma once

#include "core/math/Cubic.h"
#include "core/math/Polynomial.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace stable_polynomial_detail {
  template<class Real>
  inline Real absMax(Real a, Real b, Real c, Real d, Real e) {
    return std::max({std::abs(a), std::abs(b), std::abs(c), std::abs(d), std::abs(e)});
  }

  template<class Real>
  inline Real evalQuartic(Real a, Real b, Real c, Real d, Real e, Real x) {
    return ((((a * x) + b) * x + c) * x + d) * x + e;
  }

  template<class Iterator>
  inline void insertionSort(Iterator begin, Iterator end) {
    for (Iterator it = begin + 1; it < end; ++it) {
      auto value = *it;
      Iterator scan = it;
      while (scan > begin && value < *(scan - 1)) {
        *scan = *(scan - 1);
        --scan;
      }
      *scan = value;
    }
  }

  template<class Real>
  inline Real valueTolerance(Real scale, Real x) {
    Real magnitude = std::max(Real(1), std::abs(x));
    return std::numeric_limits<Real>::epsilon() * Real(4096) * scale * magnitude * magnitude *
           magnitude * magnitude;
  }

  template<class Real>
  inline bool nearZeroValue(Real value, Real scale, Real x) {
    return std::abs(value) <= valueTolerance(scale, x);
  }

  template<class Real>
  inline void addUniqueRoot(SortedResult<Real, 4>& roots, Real root) {
    if (!std::isfinite(static_cast<double>(root)) || roots.count == roots.values.size()) {
      return;
    }

    Real tolerance =
      std::numeric_limits<Real>::epsilon() * Real(1024) * std::max(Real(1), std::abs(root));
    for (std::size_t i = 0; i < roots.count; ++i) {
      if (std::abs(roots.values[i] - root) <= tolerance) {
        roots.values[i] = (roots.values[i] + root) / Real(2);
        return;
      }
    }
    roots.values[roots.count++] = root;
  }

  template<class Real>
  inline Real bisectQuarticRoot(Real a, Real b, Real c, Real d, Real e, Real left, Real right,
                                Real fLeft) {
    for (int i = 0; i < 128; ++i) {
      Real mid = (left + right) / Real(2);
      Real fMid = evalQuartic(a, b, c, d, e, mid);

      if (fMid == Real(0)) {
        return mid;
      }

      if ((fLeft < 0 && fMid > 0) || (fLeft > 0 && fMid < 0)) {
        right = mid;
      } else {
        left = mid;
        fLeft = fMid;
      }
    }
    return (left + right) / Real(2);
  }

  template<class Real>
  inline SortedResult<Real, 3> derivativeRoots(Real a, Real b, Real c, Real d) {
    Cubic<Real> derivative(Real(4) * a, Real(3) * b, Real(2) * c, d);
    return derivative.sortedResult();
  }
}

/**
  * Numerically conservative root finders for cases where the closed-form
  * polynomial solvers lose too much precision.
  */
template<class T>
class StablePolynomial {
public:
  static SortedResult<T, 4> solveQuartic(T a, T b, T c, T d, T e) {
    using Real = long double;
    Real ar = static_cast<Real>(a);
    Real br = static_cast<Real>(b);
    Real cr = static_cast<Real>(c);
    Real dr = static_cast<Real>(d);
    Real er = static_cast<Real>(e);

    SortedResult<Real, 4> realRoots = solveQuarticReal(ar, br, cr, dr, er);
    SortedResult<T, 4> roots;
    for (std::size_t i = 0; i < realRoots.count; ++i) {
      roots.values[roots.count++] = static_cast<T>(realRoots.values[i]);
    }
    return roots;
  }

private:
  static SortedResult<long double, 4> solveQuarticReal(long double a, long double b, long double c,
                                                       long double d, long double e) {
    using Real = long double;
    using namespace stable_polynomial_detail;

    SortedResult<Real, 4> roots;
    Real coefficientScale = absMax(a, b, c, d, e);
    if (coefficientScale == Real(0)) {
      return roots;
    }

    if (std::abs(a) <= std::numeric_limits<Real>::epsilon() * coefficientScale) {
      Cubic<Real> cubic(b, c, d, e);
      auto cubicRoots = cubic.sortedResult();
      for (Real root : cubicRoots) {
        addUniqueRoot(roots, root);
      }
      return roots;
    }

    Real invA = Real(1) / std::abs(a);
    Real bound = Real(1) + std::max({std::abs(b) * invA, std::abs(c) * invA, std::abs(d) * invA,
                                     std::abs(e) * invA});

    std::array<Real, 6> points{};
    std::size_t count = 0;
    points[count++] = -bound;

    auto critical = derivativeRoots(a, b, c, d);
    for (Real root : critical) {
      if (root > -bound && root < bound) {
        points[count++] = root;
      }
    }

    points[count++] = bound;
    insertionSort(points.begin(), points.begin() + count);

    for (std::size_t i = 0; i < count; ++i) {
      Real x = points[i];
      Real fx = evalQuartic(a, b, c, d, e, x);
      if (nearZeroValue(fx, coefficientScale, x)) {
        addUniqueRoot(roots, x);
      }
    }

    for (std::size_t i = 0; i + 1 < count; ++i) {
      Real left = points[i];
      Real right = points[i + 1];
      if (left == right) {
        continue;
      }

      Real fLeft = evalQuartic(a, b, c, d, e, left);
      Real fRight = evalQuartic(a, b, c, d, e, right);
      if (nearZeroValue(fLeft, coefficientScale, left) ||
          nearZeroValue(fRight, coefficientScale, right)) {
        continue;
      }

      if ((fLeft < 0 && fRight > 0) || (fLeft > 0 && fRight < 0)) {
        addUniqueRoot(roots, bisectQuarticRoot(a, b, c, d, e, left, right, fLeft));
      }
    }

    insertionSort(roots.begin(), roots.end());
    return roots;
  }
};
