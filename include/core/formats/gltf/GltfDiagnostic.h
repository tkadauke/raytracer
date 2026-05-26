#pragma once

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace core::gltf {

  enum class DiagnosticSeverity { Warning, Error };

  enum class DiagnosticCode {
    IoError,
    InvalidJson,
    InvalidGlb,
    UnsupportedVersion,
    MissingRequiredProperty,
    InvalidPropertyType,
    InvalidReference,
    InvalidUri,
    AssetResolutionFailed,
    BufferLengthMismatch,
    BufferViewOutOfBounds,
    InvalidAccessor,
    UnsupportedValue
  };

  struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    DiagnosticCode code = DiagnosticCode::InvalidJson;
    std::string path;
    std::string message;
    std::string reference;
    std::vector<std::string> searchedRoots;

    [[nodiscard]] std::string toString() const {
      std::ostringstream out;
      out << "glTF " << (severity == DiagnosticSeverity::Error ? "error" : "warning");
      if (!path.empty())
        out << " at " << path;
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

  class Diagnostics {
  public:
    void add(Diagnostic diagnostic) {
      m_diagnostics.push_back(std::move(diagnostic));
    }

    void error(DiagnosticCode code, std::string path, std::string message,
               std::string reference = {}) {
      Diagnostic diagnostic;
      diagnostic.severity = DiagnosticSeverity::Error;
      diagnostic.code = code;
      diagnostic.path = std::move(path);
      diagnostic.message = std::move(message);
      diagnostic.reference = std::move(reference);
      add(std::move(diagnostic));
    }

    void warning(DiagnosticCode code, std::string path, std::string message,
                 std::string reference = {}) {
      Diagnostic diagnostic;
      diagnostic.severity = DiagnosticSeverity::Warning;
      diagnostic.code = code;
      diagnostic.path = std::move(path);
      diagnostic.message = std::move(message);
      diagnostic.reference = std::move(reference);
      add(std::move(diagnostic));
    }

    [[nodiscard]] const std::vector<Diagnostic>& entries() const {
      return m_diagnostics;
    }

    [[nodiscard]] bool empty() const {
      return m_diagnostics.empty();
    }

    [[nodiscard]] bool hasErrors() const {
      for (const auto& diagnostic : m_diagnostics) {
        if (diagnostic.severity == DiagnosticSeverity::Error)
          return true;
      }
      return false;
    }

  private:
    std::vector<Diagnostic> m_diagnostics;
  };

}
