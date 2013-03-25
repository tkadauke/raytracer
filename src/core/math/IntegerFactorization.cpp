#include "core/math/IntegerFactorization.h"

using namespace std;

vector<int> IntegerFactorization::factors() const {
  int number = m_number;
  vector<int> result;
  for (int i = 2; i <= number; ++i) {
    while (number % i == 0) {
      result.push_back(i);
      number /= i;
    }
  }
  return result;
}
