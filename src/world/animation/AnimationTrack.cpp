#include "world/animation/AnimationTrack.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include <QJsonArray>
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
    return std::runtime_error(
      QString("animation track for %1: %2").arg(trackDescription(track), message).toStdString());
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

      result.push_back({requiredFrame(key), key["value"]});
    }

    return result;
  }

  QJsonArray requireTriple(const world::AnimationTrack& track, const QJsonValue& value,
                           const QString& typeName) {
    if (!value.isArray())
      throw evaluationError(track, QString("%1 key values must be arrays").arg(typeName));

    const auto array = value.toArray();
    if (array.size() != 3)
      throw evaluationError(
        track, QString("%1 key values must have exactly three elements").arg(typeName));

    for (const auto& component : array) {
      if (!component.isDouble())
        throw evaluationError(track,
                              QString("%1 key values must contain only numbers").arg(typeName));
    }

    return array;
  }

  double doubleFromJson(const world::AnimationTrack& track, const QJsonValue& value) {
    if (!value.isDouble())
      throw evaluationError(track, "double key values must be numbers");
    return value.toDouble();
  }

  int intFromJson(const world::AnimationTrack& track, const QJsonValue& value) {
    if (!value.isDouble())
      throw evaluationError(track, "integer key values must be integers");

    const auto number = value.toDouble();
    if (std::floor(number) != number)
      throw evaluationError(track, "integer key values must be integers");

    return static_cast<int>(number);
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

  QString stringFromJson(const world::AnimationTrack& track, const QJsonValue& value) {
    if (!value.isString())
      throw evaluationError(track, "string key values must be strings");
    return value.toString();
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
      keyframes.push_back({keyframe.frame, converter(track, keyframe.value)});
    }

    core::animation::AnimationTrack<Value> typedTrack(keyframes, track.interpolationMode());
    return variantFromValue(typedTrack.sample(frame));
  }

  template<class Value, class Converter>
  QVariant sampleStepOnlyTrack(const world::AnimationTrack& track, int frame,
                               const QString& typeName, Converter&& converter) {
    if (track.interpolationMode() != InterpolationMode::Step)
      throw evaluationError(track,
                            QString("%1 properties support only step interpolation").arg(typeName));

    return sampleTypedTrack<Value>(track, frame, std::forward<Converter>(converter));
  }

} // namespace

namespace world {

  AnimationTrack::AnimationTrack(QString targetId, QString propertyName,
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

    std::sort(m_keyframes.begin(), m_keyframes.end(),
              [](const auto& left, const auto& right) { return left.frame < right.frame; });

    const auto duplicate = std::adjacent_find(
      m_keyframes.begin(), m_keyframes.end(),
      [](const auto& left, const auto& right) { return left.frame == right.frame; });
    if (duplicate != m_keyframes.end())
      throw std::invalid_argument("animation track contains duplicate keyframe frames");
  }

  AnimationTrack AnimationTrack::read(const QJsonObject& json) {
    const auto interpolationName =
      json.contains("interpolation") ? requiredString(json, "interpolation") : QString("linear");
    const auto interpolationMode = InterpolationModes::fromName(interpolationName.toStdString());

    return AnimationTrack(requiredString(json, "target"), requiredString(json, "property"),
                          readKeyframes(json), interpolationMode);
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
    const auto propertyInfo = target.animationPropertyInfo(m_propertyName);
    if (!propertyInfo)
      throw evaluationError(*this, "target property does not exist");

    if (!propertyInfo->writable)
      throw evaluationError(*this, "target property is not writable");

    switch (propertyInfo->type) {
    case Element::AnimationPropertyType::Double:
      return sampleTypedTrack<double>(*this, frame, doubleFromJson);
    case Element::AnimationPropertyType::Integer:
      return sampleTypedTrack<int>(*this, frame, intFromJson);
    case Element::AnimationPropertyType::Vector3:
      return sampleTypedTrack<Vector3d>(*this, frame, vectorFromJson);
    case Element::AnimationPropertyType::Color:
      return sampleTypedTrack<Colord>(*this, frame, colorFromJson);
    case Element::AnimationPropertyType::Boolean:
      return sampleStepOnlyTrack<bool>(*this, frame, QStringLiteral("bool"), boolFromJson);
    case Element::AnimationPropertyType::String:
      return sampleStepOnlyTrack<QString>(*this, frame, QStringLiteral("string"), stringFromJson);
    case Element::AnimationPropertyType::Unsupported:
      break;
    }

    throw evaluationError(*this,
                          QString("unsupported property type '%1'").arg(propertyInfo->typeName));
  }

  void AnimationTrack::apply(Element& root, int frame) const {
    auto* target = root.findById(m_targetId);
    if (!target)
      throw evaluationError(*this, "target element was not found");

    const auto value = sample(*target, frame);
    if (!target->setAnimatedProperty(m_propertyName, value))
      throw evaluationError(*this, "sampled value could not be applied");
  }

} // namespace world
