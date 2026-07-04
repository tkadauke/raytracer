#pragma once

#include <QString>
#include <string>

inline QString qstr(const std::string& value) {
  return QString::fromStdString(value);
}

inline QString qstr(const char* value) {
  return QString::fromUtf8(value);
}
