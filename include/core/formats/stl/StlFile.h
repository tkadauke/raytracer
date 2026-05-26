#pragma once

#include "core/Exception.h"

#include <iosfwd>
#include <string>

class Mesh;

class StlParseError : public Exception {
public:
  inline explicit StlParseError(const std::string& message, const std::string& file, int line)
      : Exception(message, file, line) {
  }
};

class StlFile {
public:
  explicit StlFile(std::istream& input, Mesh& mesh);

  void read(std::istream& input, Mesh& mesh);
};
