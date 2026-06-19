#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"

#include <QJsonArray>
#include <QJsonValue>

#include <optional>
#include <utility>

namespace core::json {

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

  inline QJsonArray vector3ToJsonArray(const Vector3d& value) {
    return QJsonArray{value.x(), value.y(), value.z()};
  }

  inline QJsonArray colorToJsonArray(const Colord& value) {
    return QJsonArray{value.r(), value.g(), value.b()};
  }

  inline Vector3d vector3FromJsonArray(const QJsonArray& array) {
    return Vector3d(array[0].toDouble(), array[1].toDouble(), array[2].toDouble());
  }

  inline Colord colorFromJsonArray(const QJsonArray& array) {
    return Colord(array[0].toDouble(), array[1].toDouble(), array[2].toDouble());
  }

}
