#pragma once

#include <QString>

namespace world {

  enum class ImportDiagnosticSeverity { Warning, Error };

  /**
    * A warning or import-blocking error reported while reading an external
    * scene file. Line and column are one-based when known, or -1 when the
    * source format cannot provide that location.
    */
  struct ImportDiagnostic {
    ImportDiagnosticSeverity severity{ImportDiagnosticSeverity::Warning};
    QString message;
    QString source;
    int line{-1};
    int column{-1};

    [[nodiscard]] static ImportDiagnostic warning(const QString& message,
                                                  const QString& source = QString(),
                                                  int line = -1, int column = -1) {
      return {ImportDiagnosticSeverity::Warning, message, source, line, column};
    }

    [[nodiscard]] static ImportDiagnostic error(const QString& message,
                                                const QString& source = QString(),
                                                int line = -1, int column = -1) {
      return {ImportDiagnosticSeverity::Error, message, source, line, column};
    }

    [[nodiscard]] bool isWarning() const {
      return severity == ImportDiagnosticSeverity::Warning;
    }

    [[nodiscard]] bool isError() const {
      return severity == ImportDiagnosticSeverity::Error;
    }
  };

}
