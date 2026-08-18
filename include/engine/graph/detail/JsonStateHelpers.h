#pragma once

#include "core/json/JsonValue.h"
#include "core/math/Number.h"
#include "core/math/Rect.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
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
      if (!core::json::isIntegerValued(number))
        error(path + "." + key + "[" + std::to_string(i) + "]", "expected integer");
      values[i] = static_cast<int>(number);
    }
    const Recti rect(values);
    if (!rect.hasNonNegativeSize())
      error(path + "." + key, "width and height must be non-negative");
    return rect;
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

  // ---- JSON field extraction helpers ----------------------------------------
  //
  // Each helper takes an `error` callable with signature
  //   void error(const std::string& path, const std::string& message)
  // that is expected to throw on malformed input.

  [[nodiscard]] inline bool hasField(const QJsonObject& object, const char* key) {
    return !object.value(key).isUndefined();
  }

  /// Clamps a level-of-detail value to the non-negative range shared by every
  /// `setLod()` on the rasterizer/wireframe pass-state and options types.
  [[nodiscard]] inline int clampedLod(int lod) {
    return atLeast(0, lod);
  }

  template<typename Error>
  inline void rejectUnknownFields(const QJsonObject& object, const std::string& path,
                                  std::initializer_list<const char*> allowed, Error error) {
    for (auto it = object.begin(); it != object.end(); ++it) {
      const std::string key = it.key().toStdString();
      const bool matched = std::find(allowed.begin(), allowed.end(), key) != allowed.end();
      if (!matched)
        error(path + "." + key, "unknown field");
    }
  }

  template<typename Error>
  [[nodiscard]] inline QJsonObject objectField(const QJsonObject& object, const char* key,
                                               const std::string& path, Error error) {
    const auto value = object.value(key);
    if (value.isUndefined())
      return {};
    if (!value.isObject())
      error(path + "." + key, "expected object");
    return value.toObject();
  }

  template<typename Error>
  [[nodiscard]] inline int intField(const QJsonObject& object, const char* key,
                                    const std::string& path, Error error) {
    const auto value = object.value(key);
    if (!value.isDouble())
      error(path + "." + key, "expected integer");
    const double number = value.toDouble();
    if (!core::json::isIntegerValued(number))
      error(path + "." + key, "expected integer");
    return static_cast<int>(number);
  }

  template<typename Error>
  [[nodiscard]] inline std::uint64_t uint64Field(const QJsonObject& object, const char* key,
                                                  const std::string& path, Error error) {
    constexpr std::uint64_t maxExact = 9007199254740991ULL;
    const auto value = object.value(key);
    if (!value.isDouble())
      error(path + "." + key, "expected non-negative integer");
    const double number = value.toDouble();
    if (!core::json::isIntegerValued(number) || number < 0.0 ||
        number > static_cast<double>(maxExact))
      error(path + "." + key, "expected non-negative integer");
    return static_cast<std::uint64_t>(number);
  }

  template<typename Error>
  [[nodiscard]] inline std::uint8_t byteField(const QJsonObject& object, const char* key,
                                               const std::string& path, Error error) {
    const int v = intField(object, key, path, error);
    if (v < 0 || v > 255)
      error(path + "." + key, "expected integer from 0 to 255");
    return static_cast<std::uint8_t>(v);
  }

  template<typename Error>
  [[nodiscard]] inline double doubleField(const QJsonObject& object, const char* key,
                                          const std::string& path, Error error) {
    const auto value = object.value(key);
    if (!value.isDouble())
      error(path + "." + key, "expected number");
    const double number = value.toDouble();
    if (!std::isfinite(number))
      error(path + "." + key, "expected finite number");
    return number;
  }

  template<typename Error>
  [[nodiscard]] inline bool boolField(const QJsonObject& object, const char* key,
                                      const std::string& path, Error error) {
    const auto value = object.value(key);
    if (!value.isBool())
      error(path + "." + key, "expected boolean");
    return value.toBool();
  }

  template<typename Error>
  [[nodiscard]] inline std::string stringField(const QJsonObject& object, const char* key,
                                               const std::string& path, Error error) {
    const auto value = object.value(key);
    if (!value.isString())
      error(path + "." + key, "expected string");
    return value.toString().toStdString();
  }

  template<class T, typename Error>
  [[nodiscard]] inline T enumValue(const std::string& value,
                                   std::initializer_list<std::pair<const char*, T>> values,
                                   const std::string& path, Error error) {
    for (const auto& [name, parsed] : values) {
      if (value == name)
        return parsed;
    }
    error(path, "unknown value '" + value + "'");
    throw std::logic_error("unreachable");
  }

  template<class T>
  [[nodiscard]] inline const char* enumName(T value,
                                             std::initializer_list<std::pair<T, const char*>> values,
                                             const char* fallback = "unknown") {
    for (const auto& [parsed, name] : values) {
      if (value == parsed)
        return name;
    }
    return fallback;
  }
}
