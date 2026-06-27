#pragma once

#include "core/json/JsonValue.h"
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
    const auto values = core::json::requireNumberArray<3>(
      value, "expected [r, g, b]", "expected three numbers", "expected number",
      [&](std::optional<int> index, const char* message) {
        error(index ? path + "." + key + "[" + std::to_string(*index) + "]" : path + "." + key,
              message);
      });
    if (!values)
      error(path + "." + key, "expected [r, g, b]");

    for (int i = 0; i != 3; ++i) {
      if (!std::isfinite((*values)[static_cast<std::size_t>(i)]) ||
          (*values)[static_cast<std::size_t>(i)] < 0.0 ||
          (*values)[static_cast<std::size_t>(i)] > 1.0) {
        error(path + "." + key + "[" + std::to_string(i) + "]", "expected number from 0 to 1");
      }
    }
    return Colord(*values);
  }

  [[nodiscard]] inline QJsonArray colorToJson(const Colord& color) {
    return core::json::colorToJsonArray(color);
  }
}
