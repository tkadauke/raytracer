#include "core/formats/ldraw/LDrawParser.h"

#include "core/formats/ldraw/LDrawParseError.h"

#include <cerrno>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <utility>

using namespace std;

namespace {
  struct Token {
    string text;
    size_t begin = 0;
    size_t end = 0;
  };

  string trimRight(const string& value) {
    size_t end = value.size();
    while (end > 0 && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r'))
      --end;
    return value.substr(0, end);
  }

  string trim(const string& value) {
    size_t begin = 0;
    while (begin < value.size() && (value[begin] == ' ' || value[begin] == '\t' || value[begin] == '\r'))
      ++begin;
    return trimRight(value.substr(begin));
  }

  vector<Token> tokenize(const string& line) {
    vector<Token> tokens;
    size_t cursor = 0;
    while (cursor < line.size()) {
      while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t' || line[cursor] == '\r'))
        ++cursor;

      if (cursor == line.size())
        break;

      const size_t begin = cursor;
      while (cursor < line.size() && line[cursor] != ' ' && line[cursor] != '\t' && line[cursor] != '\r')
        ++cursor;

      tokens.push_back(Token{line.substr(begin, cursor - begin), begin, cursor});
    }
    return tokens;
  }

  [[noreturn]] void throwParseError(int lineNumber, const string& detail) {
    throw LDrawParseError(lineNumber, detail, __FILE__, __LINE__);
  }

  int parseInt(const Token& token, int lineNumber, const string& fieldName) {
    char* end = nullptr;
    errno = 0;
    int base = 10;
    if (token.text.size() > 2 && token.text[0] == '0' && (token.text[1] == 'x' || token.text[1] == 'X'))
      base = 16;
    const long value = strtol(token.text.c_str(), &end, base);
    if (errno != 0 || end == token.text.c_str() || *end != '\0' ||
        value < numeric_limits<int>::min() || value > numeric_limits<int>::max())
      throwParseError(lineNumber, "invalid integer for " + fieldName + ": '" + token.text + "'");
    return static_cast<int>(value);
  }

  double parseDouble(const Token& token, int lineNumber, const string& fieldName) {
    char* end = nullptr;
    errno = 0;
    const double value = strtod(token.text.c_str(), &end);
    if (errno != 0 || end == token.text.c_str() || *end != '\0')
      throwParseError(lineNumber, "invalid number for " + fieldName + ": '" + token.text + "'");
    return value;
  }

  void requireFieldCount(const vector<Token>& tokens, int lineNumber, size_t expected) {
    if (tokens.size() != expected) {
      ostringstream message;
      message << "line type " << tokens.front().text << " expects " << (expected - 1)
              << " fields, got " << (tokens.size() - 1);
      throwParseError(lineNumber, message.str());
    }
  }

  Vector3d parsePoint(const vector<Token>& tokens, int lineNumber, size_t begin, const string& fieldName) {
    return Vector3d(parseDouble(tokens[begin], lineNumber, fieldName + ".x"),
                    parseDouble(tokens[begin + 1], lineNumber, fieldName + ".y"),
                    parseDouble(tokens[begin + 2], lineNumber, fieldName + ".z"));
  }

  vector<string> tokenTextsAfterFirst(const vector<Token>& tokens) {
    vector<string> values;
    for (size_t i = 2; i < tokens.size(); ++i)
      values.push_back(tokens[i].text);
    return values;
  }

  string lineRemainderAfterToken(const string& line, const Token& token) {
    size_t cursor = token.end;
    while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t' || line[cursor] == '\r'))
      ++cursor;
    return trimRight(line.substr(cursor));
  }

  bool isMetaKeyword(const LDrawCommand& command, const string& keyword) {
    if (!holds_alternative<LDrawMetaCommand>(command))
      return false;
    return get<LDrawMetaCommand>(command).keyword == keyword;
  }

  string fileNameFromMeta(const LDrawMetaCommand& command) {
    return trim(command.text.substr(command.keyword.size()));
  }
}

LDrawParser::Commands LDrawParser::parse(istream& input) const {
  Commands commands;
  string line;
  int lineNumber = 1;
  while (getline(input, line)) {
    commands.push_back(parseLine(line, lineNumber));
    ++lineNumber;
  }
  return commands;
}

bool LDrawDocument::isMultipart() const {
  return files.size() > 1 || (!files.empty() && !files.front().filename.empty());
}

const LDrawDocumentFile& LDrawDocument::mainFile() const {
  return files.front();
}

LDrawDocument LDrawParser::parseDocument(istream& input) const {
  vector<pair<LDrawCommand, string>> parsedLines;
  bool hasFileBlocks = false;
  string line;
  int lineNumber = 1;
  while (getline(input, line)) {
    auto command = parseLine(line, lineNumber);
    if (isMetaKeyword(command, "FILE"))
      hasFileBlocks = true;
    parsedLines.emplace_back(std::move(command), line + "\n");
    ++lineNumber;
  }

  LDrawDocument document;
  if (!hasFileBlocks) {
    LDrawDocumentFile file;
    for (const auto& parsedLine : parsedLines) {
      file.commands.push_back(parsedLine.first);
      file.sourceText += parsedLine.second;
    }
    document.files.push_back(std::move(file));
    return document;
  }

  LDrawDocumentFile* currentFile = nullptr;
  for (const auto& parsedLine : parsedLines) {
    const auto& command = parsedLine.first;
    if (isMetaKeyword(command, "FILE")) {
      const auto& meta = get<LDrawMetaCommand>(command);
      document.files.push_back(LDrawDocumentFile{fileNameFromMeta(meta), {}, {}});
      currentFile = &document.files.back();
      continue;
    }

    if (isMetaKeyword(command, "NOFILE")) {
      currentFile = nullptr;
      continue;
    }

    if (currentFile) {
      currentFile->commands.push_back(command);
      currentFile->sourceText += parsedLine.second;
    }
  }

  return document;
}

LDrawCommand LDrawParser::parseLine(const string& line, int lineNumber) const {
  const vector<Token> tokens = tokenize(line);
  if (tokens.empty())
    return LDrawEmptyLine{lineNumber};

  const string& lineType = tokens.front().text;
  if (lineType == "0") {
    LDrawMetaCommand command;
    command.lineNumber = lineNumber;
    command.text = lineRemainderAfterToken(line, tokens.front());
    if (tokens.size() > 1 && tokens[1].text != "//") {
      command.keyword = tokens[1].text;
      command.arguments = tokenTextsAfterFirst(tokens);
    }
    return command;
  }

  if (lineType == "1") {
    if (tokens.size() < 15)
      throwParseError(lineNumber, "line type 1 expects color, translation, matrix, and filename");

    LDrawSubfileReference command;
    command.lineNumber = lineNumber;
    command.color = parseInt(tokens[1], lineNumber, "color");
    command.translation = parsePoint(tokens, lineNumber, 2, "translation");
    for (size_t i = 0; i < command.matrix.size(); ++i)
      command.matrix[i] = parseDouble(tokens[5 + i], lineNumber, "matrix");
    command.filename = lineRemainderAfterToken(line, tokens[13]);
    if (command.filename.empty())
      throwParseError(lineNumber, "line type 1 missing filename");
    return command;
  }

  if (lineType == "2") {
    requireFieldCount(tokens, lineNumber, 8);
    LDrawEdgeLine command;
    command.lineNumber = lineNumber;
    command.color = parseInt(tokens[1], lineNumber, "color");
    command.points[0] = parsePoint(tokens, lineNumber, 2, "point1");
    command.points[1] = parsePoint(tokens, lineNumber, 5, "point2");
    return command;
  }

  if (lineType == "3") {
    requireFieldCount(tokens, lineNumber, 11);
    LDrawTriangle command;
    command.lineNumber = lineNumber;
    command.color = parseInt(tokens[1], lineNumber, "color");
    command.points[0] = parsePoint(tokens, lineNumber, 2, "point1");
    command.points[1] = parsePoint(tokens, lineNumber, 5, "point2");
    command.points[2] = parsePoint(tokens, lineNumber, 8, "point3");
    return command;
  }

  if (lineType == "4") {
    requireFieldCount(tokens, lineNumber, 14);
    LDrawQuad command;
    command.lineNumber = lineNumber;
    command.color = parseInt(tokens[1], lineNumber, "color");
    command.points[0] = parsePoint(tokens, lineNumber, 2, "point1");
    command.points[1] = parsePoint(tokens, lineNumber, 5, "point2");
    command.points[2] = parsePoint(tokens, lineNumber, 8, "point3");
    command.points[3] = parsePoint(tokens, lineNumber, 11, "point4");
    return command;
  }

  if (lineType == "5") {
    requireFieldCount(tokens, lineNumber, 14);
    LDrawOptionalLine command;
    command.lineNumber = lineNumber;
    command.color = parseInt(tokens[1], lineNumber, "color");
    command.points[0] = parsePoint(tokens, lineNumber, 2, "point1");
    command.points[1] = parsePoint(tokens, lineNumber, 5, "point2");
    command.points[2] = parsePoint(tokens, lineNumber, 8, "control1");
    command.points[3] = parsePoint(tokens, lineNumber, 11, "control2");
    return command;
  }

  return LDrawUnknownCommand{lineNumber, lineType, trimRight(line)};
}
