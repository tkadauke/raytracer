#include "world/import/ImportDiagnostic.h"

#include <QJsonObject>

namespace world {

  QString ImportDiagnostic::severityName() const {
    return isError() ? QStringLiteral("error") : QStringLiteral("warning");
  }

  QJsonObject ImportDiagnostic::toJson() const {
    QJsonObject json;
    json["severity"] = severityName();
    json["message"] = message;
    if (!source.isEmpty())
      json["source"] = source;
    if (line >= 0)
      json["line"] = line;
    if (column >= 0)
      json["column"] = column;
    return json;
  }

  ImportDiagnostic ImportDiagnostic::fromJson(const QJsonObject& json) {
    ImportDiagnostic diagnostic;
    diagnostic.severity = json["severity"].toString() == QStringLiteral("error")
      ? ImportDiagnosticSeverity::Error
      : ImportDiagnosticSeverity::Warning;
    diagnostic.message = json["message"].toString();
    diagnostic.source = json["source"].toString();
    diagnostic.line = json.contains("line") ? json["line"].toInt(-1) : -1;
    diagnostic.column = json.contains("column") ? json["column"].toInt(-1) : -1;
    return diagnostic;
  }

}
