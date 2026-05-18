#pragma once

#include <limits>
#include <algorithm>
#include <array>

/**
  * Stack-allocated sorted result from a polynomial solve. Avoids heap
  * allocation on the hot intersection path. Supports the same size(),
  * operator[], and begin()/end() interface as std::vector so callers
  * that use range-for or index access need no changes.
  */
template<class T, int N>
struct SortedResult {
  using value_type = T;
  using const_iterator = const T*;

  std::array<T, N> values{};
  std::size_t count = 0;

  std::size_t size() const { return count; }
  bool empty() const { return count == 0; }
  const T& operator[](std::size_t i) const { return values[i]; }
  T& operator[](std::size_t i) { return values[i]; }
  const T* begin() const { return values.data(); }
  const T* end() const { return values.data() + count; }
  T* begin() { return values.data(); }
  T* end() { return values.data() + count; }
};

/**
  * CRTP base class for solving polynomials. Derived must define solve().
  *
  * The third template parameter (Derived) enables static dispatch: calls to
  * solveInto() and sortedResult() resolve to Derived::solve() at compile time,
  * eliminating the vtable pointer and indirect call that existed when solve()
  * was virtual. The polynomial degree is compile-time-known at every call site
  * so virtual dispatch was unnecessary overhead.
  */
template<class T, int Dimension, class Derived>
class Polynomial {
public:
  typedef T Coefficient;
  typedef T Result[Dimension];

  /**
    * Constructor. Initializes the result vector with NaN values.
    */
  inline explicit Polynomial() {
    for (int i = 0; i != Dimension; ++i) {
      m_result[i] = std::numeric_limits<T>::quiet_NaN();
    }
  }

  /**
    * Solves the polynomial into the given @p resultArray. Only the number of
    * elements that correspond to the number of real solutions are modified in
    * the array. All other elements are undefined after calling this method.
    *
    * @returns the number of solutions.
    */
  inline int solveInto(T* resultArray) {
    int num = static_cast<Derived*>(this)->solve();
    for (int i = 0; i != num; ++i) {
      *resultArray++ = m_result[i];
    }
    return num;
  }

  /**
    * @returns a reference to the result array. You need to call solve() before
    *   any of the values in the result array are set. Only as many elements as
    *   there are solutions will be set in the result array.
    */
  inline const Result& result() const {
    return m_result;
  }

  /**
    * @returns the sorted real roots in a stack-allocated SortedResult (no heap
    *   allocation). There is no need to call solve() first.
    */
  inline SortedResult<T, Dimension> sortedResult() {
    SortedResult<T, Dimension> res;
    int num = static_cast<Derived*>(this)->solve();
    for (int i = 0; i != num; ++i) {
      res.values[res.count++] = m_result[i];
    }
    // GCC 12 inlines std::sort's insertion-sort path (threshold 16) and emits
    // a false-positive -Warray-bounds for the fixed-size array.  The access is
    // dead code for N <= 16; suppress the diagnostic locally.
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Warray-bounds"
#endif
    std::sort(res.begin(), res.end());
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
    return res;
  }

protected:
  Result m_result;
};
