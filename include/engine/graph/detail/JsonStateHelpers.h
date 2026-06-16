#pragma once

#include "core/Color.h"
#include "core/math/Rect.h"

#include <QJsonArray>
#include <QJsonObject>

#include <cmath>
#include <string>

namespace engine::graph::detail {
  template<typename Error>
  [[nodiscard]] inline Recti rectFromJson(const QJsonObject& object, const char* key,
                                          const std::string& path, Error error) {
    const auto value = object.value(key);
    if (!value.isArray())
      error(path + "." + key, "expected [x, y, width, height]");

    const auto array = value.toArray();
    if (array.size() != 4)
      error(path + "." + key, "expected four integers");

    int values[4];
    for (int i = 0; i != 4; ++i) {
      if (!array.at(i).isDouble())
        error(path + "." + key + "[" + std::to_string(i) + "]", "expected integer");
      const double number = array.at(i).toDouble();
      if (!std::isfinite(number) || std::floor(number) != number)
        error(path + "." + key + "[" + std::to_string(i) + "]", "expected integer");
      values[i] = static_cast<int>(number);
    }
    if (values[2] < 0 || values[3] < 0)
      error(path + "." + key, "width and height must be non-negative");
    return Recti(values[0], values[1], values[2], values[3]);
  }

  [[nodiscard]] inline QJsonArray rectToJson(const Recti& rect) {
    return QJsonArray{rect.left(), rect.top(), rect.width(), rect.height()};
  }

  template<typename Error>
  [[nodiscard]] inline Colord colorFromJson(const QJsonObject& object, const char* key,
                                            const std::string& path, Error error) {
    const auto value = object.value(key);
    if (!value.isArray())
      error(path + "." + key, "expected [r, g, b]");

    const auto array = value.toArray();
    if (array.size() != 3)
      error(path + "." + key, "expected three numbers");

    double values[3];
    for (int i = 0; i != 3; ++i) {
      if (!array.at(i).isDouble())
        error(path + "." + key + "[" + std::to_string(i) + "]", "expected number");
      values[i] = array.at(i).toDouble();
      if (!std::isfinite(values[i]) || values[i] < 0.0 || values[i] > 1.0)
        error(path + "." + key + "[" + std::to_string(i) + "]", "expected number from 0 to 1");
    }
    return Colord(values[0], values[1], values[2]);
  }

  [[nodiscard]] inline QJsonArray colorToJson(const Colord& color) {
    return QJsonArray{color.r(), color.g(), color.b()};
  }
}
