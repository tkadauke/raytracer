#pragma once

#include "core/Exception.h"

class PlyParseError : public Exception {
public:
  inline explicit PlyParseError(const std::string& file, int line)
    : Exception("Parse error in PLY file", file, line)
  {
  }
};
