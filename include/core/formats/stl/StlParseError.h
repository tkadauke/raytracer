#pragma once

#include "core/Exception.h"

#include <string>

namespace core::formats::stl {

  class StlParseError : public Exception {
  public:
    inline explicit StlParseError(const std::string& message, const std::string& file, int line)
        : Exception(message, file, line) {
    }
  };

}
