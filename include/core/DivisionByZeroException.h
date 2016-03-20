#pragma once

#include "core/Exception.h"

class DivisionByZeroException : public Exception {
public:
  inline explicit DivisionByZeroException(const std::string& file, int line)
    : Exception("Division by zero", file, line)
  {
  }
};
