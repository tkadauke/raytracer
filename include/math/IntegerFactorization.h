#ifndef INTEGER_FACTORIZATION_H
#define INTEGER_FACTORIZATION_H

#include <vector>

class IntegerFactorization {
public:
  IntegerFactorization(int number)
    : m_number(number) {}
  
  std::vector<int> factors() const;

private:
  int m_number;
};

#endif
