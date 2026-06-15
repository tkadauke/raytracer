#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"

#include <QJsonArray>

namespace core::json {

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
