#pragma once

#include "core/formats/ldraw/LDrawParseError.h"

#include <cerrno>
#include <cstdlib>
#include <limits>
#include <string>

namespace ldraw_internal {
  [[noreturn]] inline void throwParseError(int lineNumber, const std::string& detail,
                                           const char* file, int line) {
    throw LDrawParseError(lineNumber, detail, file, line);
  }

  inline int parseIntFromString(const std::string& text, int lineNumber,
                                const std::string& fieldName,
                                const char* file, int line) {
    char* end = nullptr;
    errno = 0;
    int base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
      base = 16;
    const long value = strtol(text.c_str(), &end, base);
    if (errno != 0 || end == text.c_str() || *end != '\0' ||
        value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
      throwParseError(lineNumber, "invalid integer for " + fieldName + ": '" + text + "'", file, line);
    return static_cast<int>(value);
  }
}

#define LDRAW_THROW_PARSE_ERROR(lineNumber, detail) \
  ::ldraw_internal::throwParseError((lineNumber), (detail), __FILE__, __LINE__)

#define LDRAW_PARSE_INT(text, lineNumber, fieldName) \
  ::ldraw_internal::parseIntFromString((text), (lineNumber), (fieldName), __FILE__, __LINE__)
