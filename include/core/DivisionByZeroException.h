#pragma once

#include "core/Exception.h"

/**
  * Exception representing a division by zero. Throw this exception instead of
  * dividing by zero.
  */
class DivisionByZeroException : public Exception {
public:
  /**
    * Constructor. Sets the message to "Division by zero". For @p file and
    * @p line, you can use the standard macros __FILE__ and __LINE__,
    * respectively.
    */
  inline explicit DivisionByZeroException(const std::string& file, int line)
    : Exception("Division by zero", file, line)
  {
  }
};
