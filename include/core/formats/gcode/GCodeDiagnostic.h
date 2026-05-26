#pragma once

#include <sstream>
#include <string>
#include <utility>
#include <vector>

enum class GCodeDiagnosticSeverity { Warning, Error };

enum class GCodeDiagnosticCode {
  MalformedWord,
  InvalidNumber,
  MissingValue,
  UnknownCommand,
  UnsupportedCommand
};

struct GCodeDiagnostic {
  GCodeDiagnosticSeverity severity = GCodeDiagnosticSeverity::Warning;
  GCodeDiagnosticCode code = GCodeDiagnosticCode::UnsupportedCommand;
  int lineNumber = 0;
  std::string message;
  std::string command;

  [[nodiscard]] std::string toString() const {
    std::ostringstream out;
    out << "G-code " << (severity == GCodeDiagnosticSeverity::Error ? "error" : "warning");
    if (lineNumber > 0)
      out << ':' << lineNumber;
    out << ": " << message;
    if (!command.empty())
      out << " [" << command << ']';
    return out.str();
  }
};

class GCodeDiagnostics {
public:
  void add(GCodeDiagnostic diagnostic) {
    m_diagnostics.push_back(std::move(diagnostic));
  }

  void warning(GCodeDiagnosticCode code, int lineNumber, std::string message,
               std::string command = {}) {
    GCodeDiagnostic diagnostic;
    diagnostic.severity = GCodeDiagnosticSeverity::Warning;
    diagnostic.code = code;
    diagnostic.lineNumber = lineNumber;
    diagnostic.message = std::move(message);
    diagnostic.command = std::move(command);
    add(std::move(diagnostic));
  }

  void error(GCodeDiagnosticCode code, int lineNumber, std::string message,
             std::string command = {}) {
    GCodeDiagnostic diagnostic;
    diagnostic.severity = GCodeDiagnosticSeverity::Error;
    diagnostic.code = code;
    diagnostic.lineNumber = lineNumber;
    diagnostic.message = std::move(message);
    diagnostic.command = std::move(command);
    add(std::move(diagnostic));
  }

  [[nodiscard]] const std::vector<GCodeDiagnostic>& entries() const {
    return m_diagnostics;
  }

  [[nodiscard]] bool empty() const {
    return m_diagnostics.empty();
  }

  [[nodiscard]] bool hasErrors() const {
    for (const auto& diagnostic : m_diagnostics) {
      if (diagnostic.severity == GCodeDiagnosticSeverity::Error)
        return true;
    }
    return false;
  }

private:
  std::vector<GCodeDiagnostic> m_diagnostics;
};
