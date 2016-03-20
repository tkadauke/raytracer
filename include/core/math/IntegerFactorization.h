#pragma once

#include <vector>

/**
  * Calculates the prime factorization of an integer \f$i\f$ and primes \f$p_1
  * \ldots p_n\f$, so that \f$ i = p_1 \times \ldots \times p_n\f$.
  */
class IntegerFactorization {
public:
  /**
    * Constructs the IntegerFactorization object for number.
    */
  inline explicit IntegerFactorization(int number)
    : m_number(number)
  {
  }
  
  /**
    * @returns a vector with all the primes that make up the number.
    */
  std::vector<int> factors() const;

private:
  int m_number;
};
