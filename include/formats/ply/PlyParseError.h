#ifndef PLY_PARSE_ERROR_H
#define PLY_PARSE_ERROR_H

#include "core/Exception.h"

class PlyParseError : public Exception {
public:
  PlyParseError(const std::string& file, int line)
    : Exception("Parse error in PLY file", file, line)
  {
  }
};

#endif
