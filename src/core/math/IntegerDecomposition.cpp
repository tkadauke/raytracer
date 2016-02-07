#include "core/math/IntegerDecomposition.h"
#include "core/math/IntegerFactorization.h"

#include <cstdlib>

using namespace std;

void IntegerDecomposition::decompose() {
  vector<int> factors = IntegerFactorization(m_number).factors();
  
  m_first = 1;
  m_second = 1;
  
  for (vector<int>::size_type i = 0; i != factors.size(); ++i) {
    int first = 1, second = 1;
    
    for (vector<int>::size_type j = 0; j != i; ++j)
      first *= factors[j];
    for (vector<int>::size_type j = i; j != factors.size(); ++j)
      second *= factors[j];
    
    if (i == 0 || abs(first - second) < abs(m_first - m_second)) {
      m_first = first;
      m_second = second;
    }
  }
}
