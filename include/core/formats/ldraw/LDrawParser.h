#pragma once

#include "core/formats/ldraw/LDrawCommand.h"

#include <istream>
#include <string>
#include <vector>

struct LDrawDocumentFile {
  std::string filename;
  std::vector<LDrawCommand> commands;
  std::string sourceText;
};

struct LDrawDocument {
  std::vector<LDrawDocumentFile> files;

  [[nodiscard]] bool isMultipart() const;
  [[nodiscard]] const LDrawDocumentFile& mainFile() const;
};

class LDrawParser {
public:
  using Commands = std::vector<LDrawCommand>;

  Commands parse(std::istream& input) const;
  LDrawDocument parseDocument(std::istream& input) const;
  LDrawCommand parseLine(const std::string& line, int lineNumber) const;
};
