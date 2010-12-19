#ifndef INTEGER_DECOMPOSITION_H
#define INTEGER_DECOMPOSITION_H

class IntegerDecomposition {
public:
  IntegerDecomposition(int number)
    : m_number(number)
  {
    decompose();
  }
  
  int first() const { return m_first; }
  int second() const { return m_second; }

private:
  void decompose();
  
  int m_number;
  int m_first, m_second;
};

#endif
