#include "core/formats/gcode/GCodeParser.h"

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <optional>

using namespace std;

namespace {
  struct Word {
    char letter = '\0';
    string value;
  };

  struct ParsedLine {
    vector<Word> words;
    string comment;
  };

  string trim(const string& value) {
    size_t begin = 0;
    while (begin < value.size() && isspace(static_cast<unsigned char>(value[begin])))
      ++begin;
    size_t end = value.size();
    while (end > begin && isspace(static_cast<unsigned char>(value[end - 1])))
      --end;
    return value.substr(begin, end - begin);
  }

  string upper(const string& value) {
    string result = value;
    for (char& c : result)
      c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    return result;
  }

  ParsedLine splitLine(const string& line) {
    ParsedLine parsed;
    const size_t commentBegin = line.find(';');
    const string commandText = commentBegin == string::npos ? line : line.substr(0, commentBegin);
    if (commentBegin != string::npos)
      parsed.comment = trim(line.substr(commentBegin + 1));

    size_t cursor = 0;
    while (cursor < commandText.size()) {
      while (cursor < commandText.size() &&
             isspace(static_cast<unsigned char>(commandText[cursor])))
        ++cursor;
      if (cursor == commandText.size())
        break;

      if (!isalpha(static_cast<unsigned char>(commandText[cursor]))) {
        parsed.words.push_back(Word{'\0', commandText.substr(cursor, 1)});
        ++cursor;
        continue;
      }

      Word word;
      word.letter = static_cast<char>(toupper(static_cast<unsigned char>(commandText[cursor])));
      ++cursor;
      const size_t valueBegin = cursor;
      while (cursor < commandText.size() &&
             !isspace(static_cast<unsigned char>(commandText[cursor])) &&
             !isalpha(static_cast<unsigned char>(commandText[cursor])))
        ++cursor;
      word.value = commandText.substr(valueBegin, cursor - valueBegin);
      parsed.words.push_back(std::move(word));
    }
    return parsed;
  }

  optional<double> parseDoubleValue(const string& text) {
    if (text.empty())
      return nullopt;
    char* end = nullptr;
    errno = 0;
    const double value = strtod(text.c_str(), &end);
    if (errno != 0 || end == text.c_str() || *end != '\0')
      return nullopt;
    return value;
  }

  optional<int> parseIntValue(const string& text) {
    if (text.empty())
      return nullopt;
    char* end = nullptr;
    errno = 0;
    const long value = strtol(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0' || value < numeric_limits<int>::min() ||
        value > numeric_limits<int>::max())
      return nullopt;
    return static_cast<int>(value);
  }

  string commandName(const Word& word) {
    return string(1, word.letter) + word.value;
  }

  optional<double> parameter(const vector<Word>& words, char letter, int lineNumber,
                             GCodeDiagnostics& diagnostics, const string& command) {
    for (size_t i = 1; i < words.size(); ++i) {
      if (words[i].letter != letter)
        continue;
      const auto value = parseDoubleValue(words[i].value);
      if (!value) {
        diagnostics.warning(GCodeDiagnosticCode::InvalidNumber, lineNumber,
                            string("invalid numeric parameter ") + letter + words[i].value,
                            command);
      }
      return value;
    }
    return nullopt;
  }

  bool hasMotionParameter(const vector<Word>& words) {
    for (size_t i = 1; i < words.size(); ++i) {
      if (words[i].letter == 'X' || words[i].letter == 'Y' || words[i].letter == 'Z' ||
          words[i].letter == 'E')
        return true;
    }
    return false;
  }

  template<class State>
  void recordMetadataComment(const string& comment, int lineNumber, GCodeProgram& program,
                             State& state) {
    if (comment.empty())
      return;

    program.comments.push_back(GCodeComment{lineNumber, comment});
    const string normalized = upper(comment);
    const size_t colon = comment.find(':');
    if (colon != string::npos) {
      const string key = trim(comment.substr(0, colon));
      const string value = trim(comment.substr(colon + 1));
      program.metadata.push_back(GCodeMetadata{lineNumber, key, value});

      const string upperKey = upper(key);
      if (upperKey == "LAYER") {
        const auto index = parseIntValue(value);
        if (index)
          state.layerIndex = *index;
        program.layers.push_back(
          GCodeLayer{lineNumber, state.layerIndex, state.position.z, comment});
      } else if (upperKey == "Z") {
        const auto z = parseDoubleValue(value);
        if (z) {
          state.lastLayerZ = *z;
          program.layers.push_back(GCodeLayer{lineNumber, state.layerIndex, *z, comment});
        }
      } else if (upperKey == "TYPE" || upperKey == "FEATURE") {
        state.featureType = value;
      }
      return;
    }

    if (normalized == "LAYER_CHANGE") {
      ++state.layerIndex;
      program.layers.push_back(GCodeLayer{lineNumber, state.layerIndex, state.position.z, comment});
    }
  }

  template<class State>
  void addZLayerIfNeeded(int lineNumber, double z, GCodeProgram& program, State& state) {
    if (state.lastLayerZ && *state.lastLayerZ == z)
      return;
    state.lastLayerZ = z;
    if (state.layerIndex < 0)
      state.layerIndex = 0;
    program.layers.push_back(GCodeLayer{lineNumber, state.layerIndex, z, {}});
  }

  template<class State>
  void parseMotion(const vector<Word>& words, int lineNumber, const string& command,
                   const string& comment, bool rapid, GCodeProgram& program, State& state) {
    const auto x = parameter(words, 'X', lineNumber, program.diagnostics, command);
    const auto y = parameter(words, 'Y', lineNumber, program.diagnostics, command);
    const auto z = parameter(words, 'Z', lineNumber, program.diagnostics, command);
    const auto e = parameter(words, 'E', lineNumber, program.diagnostics, command);
    const auto f = parameter(words, 'F', lineNumber, program.diagnostics, command);

    if (f)
      state.feedRate = *f;

    if (!hasMotionParameter(words))
      return;

    const GCodePosition previous = state.position;
    if (x)
      state.position.x = state.absolutePositioning ? *x : state.position.x + *x;
    if (y)
      state.position.y = state.absolutePositioning ? *y : state.position.y + *y;
    if (z)
      state.position.z = state.absolutePositioning ? *z : state.position.z + *z;
    if (e)
      state.position.e = state.absoluteExtrusion ? *e : state.position.e + *e;

    if (z && *z != previous.z)
      addZLayerIfNeeded(lineNumber, state.position.z, program, state);

    GCodeMotion motion;
    motion.lineNumber = lineNumber;
    motion.rapid = rapid;
    motion.start = Vector3d(previous.x, previous.y, previous.z);
    motion.end = Vector3d(state.position.x, state.position.y, state.position.z);
    motion.startExtruder = previous.e;
    motion.endExtruder = state.position.e;
    motion.extrusionDelta = state.position.e - previous.e;
    motion.feedRate = state.feedRate;
    motion.layerIndex = state.layerIndex;
    motion.tool = state.activeTool;
    motion.featureType = state.featureType;
    motion.comment = comment;
    program.motions.push_back(std::move(motion));
  }

  template<class State>
  void setPosition(const vector<Word>& words, int lineNumber, const string& command,
                   GCodeProgram& program, State& state) {
    const auto x = parameter(words, 'X', lineNumber, program.diagnostics, command);
    const auto y = parameter(words, 'Y', lineNumber, program.diagnostics, command);
    const auto z = parameter(words, 'Z', lineNumber, program.diagnostics, command);
    const auto e = parameter(words, 'E', lineNumber, program.diagnostics, command);
    if (x)
      state.position.x = *x;
    if (y)
      state.position.y = *y;
    if (z)
      state.position.z = *z;
    if (e)
      state.position.e = *e;
  }

  template<class State>
  void parseTemperature(const vector<Word>& words, int lineNumber, const string& command,
                        GCodeTemperatureTarget target, bool wait, GCodeProgram& program,
                        const State& state) {
    const auto temperature = parameter(words, 'S', lineNumber, program.diagnostics, command);
    if (!temperature) {
      program.diagnostics.warning(GCodeDiagnosticCode::MissingValue, lineNumber,
                                  command + " missing S temperature", command);
      return;
    }

    GCodeTemperatureCommand result;
    result.lineNumber = lineNumber;
    result.target = target;
    result.wait = wait;
    result.tool = static_cast<int>(
      parameter(words, 'T', lineNumber, program.diagnostics, command).value_or(state.activeTool));
    result.temperature = *temperature;
    program.temperatures.push_back(result);
  }
}

GCodeProgram GCodeParser::parse(istream& input) const {
  GCodeProgram program;
  m_state = State{};
  string line;
  int lineNumber = 1;
  while (getline(input, line)) {
    parseLine(line, lineNumber, program);
    ++lineNumber;
  }
  return program;
}

void GCodeParser::parseLine(const string& line, int lineNumber, GCodeProgram& program) const {
  State& state = m_state;
  const ParsedLine parsed = splitLine(line);
  recordMetadataComment(parsed.comment, lineNumber, program, state);

  if (parsed.words.empty())
    return;

  for (const auto& word : parsed.words) {
    if (word.letter == '\0') {
      program.diagnostics.warning(GCodeDiagnosticCode::MalformedWord, lineNumber,
                                  "malformed word '" + word.value + "'");
      return;
    }
  }

  const Word& first = parsed.words.front();
  const string command = commandName(first);
  const auto commandNumber = parseIntValue(first.value);
  if (!commandNumber) {
    program.diagnostics.warning(GCodeDiagnosticCode::InvalidNumber, lineNumber,
                                "invalid command number", command);
    return;
  }

  if (first.letter == 'T') {
    const auto tool = parseIntValue(first.value);
    if (!tool) {
      program.diagnostics.warning(GCodeDiagnosticCode::InvalidNumber, lineNumber,
                                  "invalid tool number", command);
      return;
    }
    state.activeTool = *tool;
    program.toolChanges.push_back(GCodeToolChange{lineNumber, *tool});
    return;
  }

  if (first.letter == 'G') {
    const int code = *commandNumber;
    switch (code) {
    case 0:
      parseMotion(parsed.words, lineNumber, command, parsed.comment, true, program, state);
      return;
    case 1:
      parseMotion(parsed.words, lineNumber, command, parsed.comment, false, program, state);
      return;
    case 90:
      state.absolutePositioning = true;
      return;
    case 91:
      state.absolutePositioning = false;
      return;
    case 92:
      setPosition(parsed.words, lineNumber, command, program, state);
      return;
    default:
      program.diagnostics.warning(GCodeDiagnosticCode::UnsupportedCommand, lineNumber,
                                  "unsupported G-code command was ignored", command);
      return;
    }
  }

  if (first.letter == 'M') {
    const int code = *commandNumber;
    switch (code) {
    case 82:
      state.absoluteExtrusion = true;
      return;
    case 83:
      state.absoluteExtrusion = false;
      return;
    case 104:
      parseTemperature(parsed.words, lineNumber, command, GCodeTemperatureTarget::Tool, false,
                       program, state);
      return;
    case 109:
      parseTemperature(parsed.words, lineNumber, command, GCodeTemperatureTarget::Tool, true,
                       program, state);
      return;
    case 140:
      parseTemperature(parsed.words, lineNumber, command, GCodeTemperatureTarget::Bed, false,
                       program, state);
      return;
    case 190:
      parseTemperature(parsed.words, lineNumber, command, GCodeTemperatureTarget::Bed, true,
                       program, state);
      return;
    default:
      program.diagnostics.warning(GCodeDiagnosticCode::UnsupportedCommand, lineNumber,
                                  "unsupported M-code command was ignored", command);
      return;
    }
  }

  program.diagnostics.warning(GCodeDiagnosticCode::UnknownCommand, lineNumber,
                              "unknown command was ignored", command);
}
