#pragma once

/**
  * Calculates the factorization of an integer \f$i = mn\f$, so that \f$m\f$
  * and \f$n\f$ are as close together as possible.
  */
class IntegerDecomposition {
public:
  /**
    * Constructs the IntegerDecomposition object and does the calculation. Call
    * first() and second() for the results.
    * 
    * @param number the integer to decompose.
    */
  inline IntegerDecomposition(int number)
    : m_number(number)
  {
    decompose();
  }
  
  /**
    * @returns the first factor.
    */
  inline int first() const {
    return m_first;
  }
  
  /**
    * @returns the second factor.
    */
  inline int second() const {
    return m_second;
  }

private:
  void decompose();
  
  int m_number;
  int m_first, m_second;
};
