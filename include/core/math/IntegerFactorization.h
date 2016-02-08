#pragma once

#include <vector>

class IntegerFactorization {
public:
  IntegerFactorization(int number)
    : m_number(number) {}
  
  std::vector<int> factors() const;

private:
  int m_number;
};
