#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"

#include <QJsonArray>
#include <QJsonValue>

#include <array>
#include <cmath>
#include <optional>
#include <utility>

namespace core::json {

  /**
    * @returns whether @p value is finite and has no fractional part, i.e. it
    *   round-trips exactly through an integral type without truncation.
    */
  inline bool isIntegerValued(double value) {
    return std::isfinite(value) && std::floor(value) == value;
  }

  template<class Message, class ErrorHandler>
  inline QJsonArray requireNumberArray(const QJsonValue& value, int expectedSize,
                                       Message arrayMessage, Message sizeMessage,
                                       Message numberMessage, ErrorHandler&& error) {
    if (!value.isArray()) {
      std::forward<ErrorHandler>(error)(std::nullopt, arrayMessage);
      return {};
    }

    const auto array = value.toArray();
    if (array.size() != expectedSize) {
      std::forward<ErrorHandler>(error)(std::nullopt, sizeMessage);
      return {};
    }

    for (int i = 0; i != expectedSize; ++i) {
      if (!array.at(i).isDouble()) {
        std::forward<ErrorHandler>(error)(i, numberMessage);
        return {};
      }
    }

    return array;
  }

  template<std::size_t Size>
  inline std::array<double, Size> numberArrayFromJsonArray(const QJsonArray& array) {
    std::array<double, Size> result{};
    for (std::size_t i = 0; i != Size; ++i)
      result[i] = array.at(static_cast<int>(i)).toDouble();
    return result;
  }

  /**
    * Converts any iterable range of numeric values (a @c std::vector, a
    * @c std::array, an @c std::initializer_list, ...) into a @c QJsonArray of
    * doubles.
    */
  template<class Container>
  inline QJsonArray numericRangeToJsonArray(const Container& values) {
    QJsonArray result;
    for (const auto& value : values)
      result.append(static_cast<double>(value));
    return result;
  }

  template<class Source, std::size_t Size>
  inline QJsonArray numberArrayToJsonArray(const std::array<Source, Size>& values) {
    return numericRangeToJsonArray(values);
  }

  template<std::size_t Size, class Message, class ErrorHandler>
  inline std::optional<std::array<double, Size>>
  requireNumberArray(const QJsonValue& value, Message arrayMessage, Message sizeMessage,
                     Message numberMessage, ErrorHandler&& error) {
    const auto array = requireNumberArray(value, static_cast<int>(Size), arrayMessage, sizeMessage,
                                          numberMessage, std::forward<ErrorHandler>(error));
    if (array.size() != static_cast<int>(Size))
      return std::nullopt;
    return numberArrayFromJsonArray<Size>(array);
  }

  inline QJsonArray vector3ToJsonArray(const Vector3d& value) {
    return numberArrayToJsonArray(value.toArray());
  }

  inline QJsonArray colorToJsonArray(const Colord& value) {
    return numberArrayToJsonArray(value.toArray());
  }

  inline Vector3d vector3FromJsonArray(const QJsonArray& array) {
    return Vector3d(numberArrayFromJsonArray<3>(array));
  }

  inline Colord colorFromJsonArray(const QJsonArray& array) {
    return Colord(numberArrayFromJsonArray<3>(array));
  }

  template<class Message, class ErrorHandler>
  inline Vector3d requireVector3(const QJsonValue& value, Message arrayMessage, Message sizeMessage,
                                 Message numberMessage, ErrorHandler&& error) {
    const auto values = requireNumberArray<3>(value, arrayMessage, sizeMessage, numberMessage,
                                              std::forward<ErrorHandler>(error));
    return values ? Vector3d(*values) : Vector3d();
  }

  template<class Message, class ErrorHandler>
  inline Colord requireColor(const QJsonValue& value, Message arrayMessage, Message sizeMessage,
                             Message numberMessage, ErrorHandler&& error) {
    const auto values = requireNumberArray<3>(value, arrayMessage, sizeMessage, numberMessage,
                                              std::forward<ErrorHandler>(error));
    return values ? Colord(*values) : Colord();
  }

}
