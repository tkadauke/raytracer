#include "world/animation/AnimationTrack.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include <QJsonArray>
#include <QMetaProperty>

#include "core/Color.h"
#include "core/animation/AnimationTrack.h"
#include "core/math/Vector.h"
#include "world/objects/Element.h"

Q_DECLARE_METATYPE(Vector3d);
Q_DECLARE_METATYPE(Colord);

namespace {

using core::math::interpolation::InterpolationMode;
using core::math::interpolation::InterpolationModes;

QString trackDescription(const world::AnimationTrack& track) {
  return QString("target '%1' property '%2'").arg(track.targetId(), track.propertyName());
}

std::invalid_argument invalidTrack(const QString& message) {
  return std::invalid_argument(message.toStdString());
}

std::runtime_error evaluationError(const world::AnimationTrack& track, const QString& message) {
  return std::runtime_error(QString("animation track for %1: %2")
                              .arg(trackDescription(track), message)
                              .toStdString());
}

QString requiredString(const QJsonObject& json, const QString& name) {
  const auto value = json[name];
  if (!value.isString())
    throw invalidTrack(QString("animation track field '%1' must be a string").arg(name));
  const auto result = value.toString();
  if (result.isEmpty())
    throw invalidTrack(QString("animation track field '%1' must not be empty").arg(name));
  return result;
}

int requiredFrame(const QJsonObject& json) {
  const auto value = json["frame"];
  if (!value.isDouble())
    throw invalidTrack("animation key field 'frame' must be an integer");

  const auto frame = value.toDouble();
  if (std::floor(frame) != frame)
    throw invalidTrack("animation key field 'frame' must be an integer");

  return static_cast<int>(frame);
}

std::vector<world::AnimationKeyframe> readKeyframes(const QJsonObject& json) {
  const auto keysValue = json["keys"];
  if (!keysValue.isArray())
    throw invalidTrack("animation track field 'keys' must be an array");

  std::vector<world::AnimationKeyframe> result;
  const auto keys = keysValue.toArray();
  result.reserve(static_cast<size_t>(keys.size()));
  for (const auto& keyValue : keys) {
    if (!keyValue.isObject())
      throw invalidTrack("animation keys must be objects");

    const auto key = keyValue.toObject();
    if (!key.contains("value"))
      throw invalidTrack("animation key is missing field 'value'");

    result.push_back({ requiredFrame(key), key["value"] });
  }

  return result;
}

QJsonArray requireTriple(const world::AnimationTrack& track, const QJsonValue& value, const QString& typeName) {
  if (!value.isArray())
    throw evaluationError(track, QString("%1 key values must be arrays").arg(typeName));

  const auto array = value.toArray();
  if (array.size() != 3)
    throw evaluationError(track, QString("%1 key values must have exactly three elements").arg(typeName));

  for (const auto& component : array) {
    if (!component.isDouble())
      throw evaluationError(track, QString("%1 key values must contain only numbers").arg(typeName));
  }

  return array;
}

double doubleFromJson(const world::AnimationTrack& track, const QJsonValue& value) {
  if (!value.isDouble())
    throw evaluationError(track, "double key values must be numbers");
  return value.toDouble();
}

Vector3d vectorFromJson(const world::AnimationTrack& track, const QJsonValue& value) {
  const auto array = requireTriple(track, value, "Vector3d");
  return Vector3d(array[0].toDouble(), array[1].toDouble(), array[2].toDouble());
}

Colord colorFromJson(const world::AnimationTrack& track, const QJsonValue& value) {
  const auto array = requireTriple(track, value, "Colord");
  return Colord(array[0].toDouble(), array[1].toDouble(), array[2].toDouble());
}

bool boolFromJson(const world::AnimationTrack& track, const QJsonValue& value) {
  if (!value.isBool())
    throw evaluationError(track, "bool key values must be booleans");
  return value.toBool();
}

template<class Value>
QVariant variantFromValue(const Value& value) {
  return QVariant::fromValue(value);
}

template<>
QVariant variantFromValue<double>(const double& value) {
  return QVariant(value);
}

template<>
QVariant variantFromValue<bool>(const bool& value) {
  return QVariant(value);
}

template<class Value, class Converter>
QVariant sampleTypedTrack(const world::AnimationTrack& track, int frame, Converter&& converter) {
  std::vector<core::animation::Keyframe<Value>> keyframes;
  keyframes.reserve(track.keyframes().size());
  for (const auto& keyframe : track.keyframes()) {
    keyframes.push_back({ keyframe.frame, converter(track, keyframe.value) });
  }

  core::animation::AnimationTrack<Value> typedTrack(keyframes, track.interpolationMode());
  return variantFromValue(typedTrack.sample(frame));
}

}  // namespace

namespace world {

AnimationTrack::AnimationTrack(QString targetId,
                               QString propertyName,
                               std::vector<AnimationKeyframe> keyframes,
                               InterpolationMode interpolationMode)
  : m_targetId(std::move(targetId)),
    m_propertyName(std::move(propertyName)),
    m_keyframes(std::move(keyframes)),
    m_interpolationMode(interpolationMode) {
  if (m_targetId.isEmpty())
    throw std::invalid_argument("animation track target must not be empty");
  if (m_propertyName.isEmpty())
    throw std::invalid_argument("animation track property must not be empty");
  if (m_propertyName.contains('.'))
    throw std::invalid_argument("animation track property must be a direct property name");
  if (m_keyframes.empty())
    throw std::invalid_argument("animation track must have at least one keyframe");

  std::sort(m_keyframes.begin(), m_keyframes.end(), [](const auto& left, const auto& right) {
    return left.frame < right.frame;
  });

  const auto duplicate = std::adjacent_find(m_keyframes.begin(), m_keyframes.end(),
                                            [](const auto& left, const auto& right) {
                                              return left.frame == right.frame;
                                            });
  if (duplicate != m_keyframes.end())
    throw std::invalid_argument("animation track contains duplicate keyframe frames");
}

AnimationTrack AnimationTrack::read(const QJsonObject& json) {
  const auto interpolationName = json.contains("interpolation")
                                   ? requiredString(json, "interpolation")
                                   : QString("linear");
  const auto interpolationMode =
    InterpolationModes::fromName(interpolationName.toStdString());

  return AnimationTrack(requiredString(json, "target"),
                        requiredString(json, "property"),
                        readKeyframes(json),
                        interpolationMode);
}

void AnimationTrack::write(QJsonObject& json) const {
  json["target"] = m_targetId;
  json["property"] = m_propertyName;
  json["interpolation"] = QString::fromStdString(InterpolationModes::name(m_interpolationMode));

  QJsonArray keys;
  for (const auto& keyframe : m_keyframes) {
    QJsonObject key;
    key["frame"] = keyframe.frame;
    key["value"] = keyframe.value;
    keys.append(key);
  }
  json["keys"] = keys;
}

const QString& AnimationTrack::targetId() const noexcept {
  return m_targetId;
}

const QString& AnimationTrack::propertyName() const noexcept {
  return m_propertyName;
}

InterpolationMode AnimationTrack::interpolationMode() const noexcept {
  return m_interpolationMode;
}

const std::vector<AnimationKeyframe>& AnimationTrack::keyframes() const noexcept {
  return m_keyframes;
}

QVariant AnimationTrack::sample(const Element& target, int frame) const {
  const auto propertyIndex = target.metaObject()->indexOfProperty(m_propertyName.toLatin1().constData());
  if (propertyIndex < 0)
    throw evaluationError(*this, "target property does not exist");

  const auto property = target.metaObject()->property(propertyIndex);
  if (!property.isWritable())
    throw evaluationError(*this, "target property is not writable");

  const QString type = property.typeName();
  if (type == "double")
    return sampleTypedTrack<double>(*this, frame, doubleFromJson);
  if (type == "Vector3<double>")
    return sampleTypedTrack<Vector3d>(*this, frame, vectorFromJson);
  if (type == "Color<double>")
    return sampleTypedTrack<Colord>(*this, frame, colorFromJson);
  if (type == "bool") {
    if (m_interpolationMode != InterpolationMode::Step)
      throw evaluationError(*this, "bool properties support only step interpolation");
    return sampleTypedTrack<bool>(*this, frame, boolFromJson);
  }

  throw evaluationError(*this, QString("unsupported property type '%1'").arg(type));
}

void AnimationTrack::apply(Element& root, int frame) const {
  auto* target = root.findById(m_targetId);
  if (!target)
    throw evaluationError(*this, "target element was not found");

  const auto value = sample(*target, frame);
  if (!target->setProperty(m_propertyName.toLatin1().constData(), value))
    throw evaluationError(*this, "sampled value could not be applied");
}

}  // namespace world
