#include "math/IntegerDecomposition.h"
#include "math/IntegerFactorization.h"

using namespace std;

void IntegerDecomposition::decompose() {
  vector<int> factors = IntegerFactorization(m_number).factors();
  
  m_first = 1;
  m_second = 1;
  
  for (int i = 0; i != factors.size(); ++i) {
    int first = 1, second = 1;
    
    for (int j = 0; j != i; ++j)
      first *= factors[j];
    for (int j = i; j != factors.size(); ++j)
      second *= factors[j];
    
    if (i == 0 || abs(first - second) < abs(m_first - m_second)) {
      m_first = first;
      m_second = second;
    }
  }
}
