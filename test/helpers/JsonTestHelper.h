#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace test::helpers {
  /** A JSON `[x, y, z]` array value. */
  inline QJsonValue vectorValue(double x, double y, double z) {
    return QJsonValue(QJsonArray({x, y, z}));
  }

  /** A JSON `[r, g, b]` array value. */
  inline QJsonValue colorValue(double r, double g, double b) {
    return QJsonValue(QJsonArray({r, g, b}));
  }

  /** A JSON animation keyframe object `{frame, value}`. */
  inline QJsonObject keyJson(int frame, const QJsonValue& value) {
    QJsonObject json;
    json["frame"] = frame;
    json["value"] = value;
    return json;
  }
}
