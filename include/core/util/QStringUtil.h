#pragma once

#include <QString>
#include <stdexcept>
#include <string>

inline QString qstr(const std::string& value) {
  return QString::fromStdString(value);
}

inline QString qstr(const char* value) {
  return QString::fromUtf8(value);
}

inline QString dashIfEmpty(const QString& value) {
  return value.isEmpty() ? QStringLiteral("-") : value;
}

inline std::invalid_argument invalidArgument(const QString& message) {
  return std::invalid_argument(message.toStdString());
}

inline QString normalizeOptionToken(QString value, bool stripSpaces = false) {
  value = value.trimmed().toLower();
  value.remove('_');
  value.remove('-');
  if (stripSpaces)
    value.remove(' ');
  return value;
}
