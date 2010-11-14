#ifndef DIVISION_BY_ZERO_EXCEPTION_H
#define DIVISION_BY_ZERO_EXCEPTION_H

#include "Exception.h"

class DivisionByZeroException : public Exception {
public:
  inline DivisionByZeroException(const std::string& file, int line)
    : Exception("Division by zero", file, line)
  {
  }
};

#endif
