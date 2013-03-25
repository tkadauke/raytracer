#ifndef POLYNOMIAL_H
#define POLYNOMIAL_H

#include <limits>
#include <algorithm>

template<class T, int Dimension>
class Polynomial {
public:
  typedef T Coefficient;
  typedef T Result[Dimension];
  typedef std::vector<T> Container;
  
  Polynomial() {
    for (int i = 0; i != Dimension; ++i) {
      m_result[i] = std::numeric_limits<T>::quiet_NaN();
    }
  }
  
  virtual int solve() = 0;
  inline int solveInto(T* resultArray) {
    int num = solve();
    for (int i = 0; i != num; ++i) {
      *resultArray++ = m_result[i];
    }
    return num;
  }
  
  inline const Result& result() const { return m_result; }
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

#endif
