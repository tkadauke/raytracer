#pragma once

#include <limits>
#include <algorithm>

/**
  * Base class for solving polynomials.
  */
template<class T, int Dimension>
class Polynomial {
public:
  typedef T Coefficient;
  typedef T Result[Dimension];
  typedef std::vector<T> Container;
  
  /**
    * Constructor. Initializes the result vector with NaN values.
    */
  inline explicit Polynomial() {
    for (int i = 0; i != Dimension; ++i) {
      m_result[i] = std::numeric_limits<T>::quiet_NaN();
    }
  }
  
  /**
    * Solves the polynomial. Override in your subclass to implement the concrete
    * solver.
    * 
    * @returns the number of solutions.
    */
  virtual int solve() = 0;
  
  /**
    * Solves the polynomial into the given @p resultArray. Only the number of
    * elements that correspond to the number of real solutions are modified in
    * the array. All other elements are undefined after calling this method.
    * 
    * @returns the number of solutions.
    */
  inline int solveInto(T* resultArray) {
    int num = solve();
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
    * @returns a vector of results which are sorted ascending. There is no need
    *   to call solve(), since this method calls it.
    */
  inline Container sortedResult() {
    Container res;
    int num = solve();
    for (int i = 0; i != num; ++i) {
      res.push_back(result()[i]);
    }
    std::sort(res.begin(), res.end());
    return res;
  }

protected:
  Result m_result;
};
