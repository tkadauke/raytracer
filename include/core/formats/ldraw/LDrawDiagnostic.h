#pragma once

#include <sstream>
#include <string>
#include <utility>
#include <vector>

enum class LDrawDiagnosticSeverity {
  Warning,
  Error
};

enum class LDrawDiagnosticCode {
  MissingSubfile,
  UnsupportedMetaCommand,
  UnsupportedLineType,
  SkippedGeometry,
  ColorFallback,
  DirectColorParseFailure,
  FatalParseError,
  BfcAmbiguity
};

struct LDrawDiagnostic {
  LDrawDiagnosticSeverity severity = LDrawDiagnosticSeverity::Warning;
  LDrawDiagnosticCode code = LDrawDiagnosticCode::SkippedGeometry;
  std::string file;
  int lineNumber = 0;
  std::string message;
  std::string reference;
  std::vector<std::string> searchedRoots;

  [[nodiscard]] std::string toString() const {
    std::ostringstream out;
    out << "LDraw " << (severity == LDrawDiagnosticSeverity::Error ? "error" : "warning");
    if (!file.empty())
      out << " in " << file;
    if (lineNumber > 0)
      out << ':' << lineNumber;
    out << ": " << message;
    if (!reference.empty())
      out << " [" << reference << ']';
    if (!searchedRoots.empty()) {
      out << " searched roots:";
      for (const auto& root : searchedRoots)
        out << ' ' << root;
    }
    return out.str();
  }
};

class LDrawDiagnostics {
public:
  void add(LDrawDiagnostic diagnostic) {
    m_diagnostics.push_back(std::move(diagnostic));
  }

  void warning(LDrawDiagnosticCode code, std::string file, int lineNumber, std::string message) {
    LDrawDiagnostic diagnostic;
    diagnostic.severity = LDrawDiagnosticSeverity::Warning;
    diagnostic.code = code;
    diagnostic.file = std::move(file);
    diagnostic.lineNumber = lineNumber;
    diagnostic.message = std::move(message);
    add(std::move(diagnostic));
  }

  void error(LDrawDiagnosticCode code, std::string file, int lineNumber, std::string message) {
    LDrawDiagnostic diagnostic;
    diagnostic.severity = LDrawDiagnosticSeverity::Error;
    diagnostic.code = code;
    diagnostic.file = std::move(file);
    diagnostic.lineNumber = lineNumber;
    diagnostic.message = std::move(message);
    add(std::move(diagnostic));
  }

  [[nodiscard]] const std::vector<LDrawDiagnostic>& entries() const {
    return m_diagnostics;
  }

  [[nodiscard]] bool empty() const {
    return m_diagnostics.empty();
  }

  [[nodiscard]] bool hasErrors() const {
    for (const auto& diagnostic : m_diagnostics) {
      if (diagnostic.severity == LDrawDiagnosticSeverity::Error)
        return true;
    }
    return false;
  }

private:
  std::vector<LDrawDiagnostic> m_diagnostics;
};
