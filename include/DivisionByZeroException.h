#include "Exception.h"

class DivisionByZeroException : public Exception {
public:
  inline DivisionByZeroException(const std::string& file, int line)
    : Exception("Division by zero", file, line)
  {
  }
};
