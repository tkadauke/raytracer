#pragma once

#include "core/formats/ldraw/LDrawCommand.h"

#include <istream>
#include <vector>

class LDrawParser {
public:
  using Commands = std::vector<LDrawCommand>;

  Commands parse(std::istream& input) const;
  LDrawCommand parseLine(const std::string& line, int lineNumber) const;
};
