#pragma once

#include "core/Exception.h"

#include <string>

class LDrawParseError : public Exception {
public:
  explicit LDrawParseError(int lineNumber, const std::string& detail, const std::string& file, int line);

  [[nodiscard]] inline int sourceLineNumber() const {
    return m_sourceLineNumber;
  }

private:
  int m_sourceLineNumber;
};
