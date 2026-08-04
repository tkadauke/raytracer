#include "world/animation/AnimationTrack.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include <QJsonArray>
#include "core/Color.h"
#include "core/animation/AnimationTrack.h"
#include "core/json/JsonValue.h"
#include "core/math/Vector.h"
#include "world/objects/Camera.h"
#include "world/objects/Element.h"
#include "world/objects/Light.h"
#include "world/objects/Material.h"
#include "world/objects/ScriptedSurface.h"
#include "world/objects/SourceAsset.h"
#include "world/objects/Transformable.h"

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

  world::AnimationTrackClassification classified(world::AnimationTrackClass trackClass,
                                                 QString diagnostic) {
    return world::AnimationTrackClassification{trackClass, std::move(diagnostic)};
  }

  bool isInterpolatable(Element::AnimationPropertyType type) {
    switch (type) {
    case Element::AnimationPropertyType::Double:
    case Element::AnimationPropertyType::Integer:
    case Element::AnimationPropertyType::Vector3:
    case Element::AnimationPropertyType::Color:
      return true;
    case Element::AnimationPropertyType::Boolean:
    case Element::AnimationPropertyType::String:
    case Element::AnimationPropertyType::Unsupported:
      return false;
    }

    return false;
  }

  bool isDynamicProperty(const Element& target, const QString& propertyName) {
    const QByteArray propertyKey = propertyName.toUtf8();
    return target.dynamicPropertyNames().contains(propertyKey);
  }

  bool isTransformProperty(const QString& propertyName) {
    return propertyName == QStringLiteral("position") ||
           propertyName == QStringLiteral("rotation") || propertyName == QStringLiteral("scale") ||
           propertyName == QStringLiteral("velocity");
  }

  bool isCameraPoseProperty(const QString& propertyName) {
    return propertyName == QStringLiteral("position") || propertyName == QStringLiteral("target");
  }

  bool isLightRuntimeProperty(const QString& propertyName) {
    return propertyName == QStringLiteral("direction") || propertyName == QStringLiteral("color") ||
           propertyName == QStringLiteral("intensity") || isTransformProperty(propertyName);
  }

  bool isRuntimeContinuousTargetProperty(const Element& target, const QString& propertyName,
                                         Element::AnimationPropertyType type) {
    if (!isInterpolatable(type))
      return false;

    if (qobject_cast<const Light*>(&target))
      return isLightRuntimeProperty(propertyName);

    if (qobject_cast<const Camera*>(&target))
      return isCameraPoseProperty(propertyName) || type == Element::AnimationPropertyType::Double;

    if (qobject_cast<const Material*>(&target))
      return type == Element::AnimationPropertyType::Double ||
             type == Element::AnimationPropertyType::Color;

    if (qobject_cast<const Transformable*>(&target))
      return isTransformProperty(propertyName);

    return false;
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
    return core::json::requireVector3(
      value, QString("Vector3d key values must be arrays"),
      QString("Vector3d key values must have exactly three elements"),
      QString("Vector3d key values must contain only numbers"),
      [&](std::optional<int>, const QString& message) { throw evaluationError(track, message); });
  }

  Colord colorFromJson(const world::AnimationTrack& track, const QJsonValue& value) {
    return core::json::requireColor(
      value, QString("Colord key values must be arrays"),
      QString("Colord key values must have exactly three elements"),
      QString("Colord key values must contain only numbers"),
      [&](std::optional<int>, const QString& message) { throw evaluationError(track, message); });
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

  template<class Converter>
  render::animation::AnimationTrack renderTrackFromKeys(const world::AnimationTrack& track,
                                                        Converter&& converter) {
    std::vector<render::animation::Keyframe> keyframes;
    keyframes.reserve(track.keyframes().size());
    for (const auto& keyframe : track.keyframes()) {
      keyframes.push_back(
        {static_cast<double>(keyframe.frame), converter(track, keyframe.value)});
    }
    return render::animation::AnimationTrack(keyframes, track.interpolationMode());
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

  AnimationTrackClassification AnimationTrack::classify(const Element& target) const {
    const auto propertyInfo = target.animationPropertyInfo(m_propertyName);
    if (!propertyInfo) {
      return classified(AnimationTrackClass::Rejected, "target property does not exist");
    }

    if (!propertyInfo->writable) {
      return classified(AnimationTrackClass::Rejected, "target property is not writable");
    }

    if (propertyInfo->type == Element::AnimationPropertyType::Unsupported) {
      return classified(
        AnimationTrackClass::Rejected,
        QString("unsupported or structural property type '%1'").arg(propertyInfo->typeName));
    }

    if (propertyInfo->type == Element::AnimationPropertyType::Boolean) {
      return classified(AnimationTrackClass::StepOnly,
                        "boolean properties require step-only frame evaluation");
    }

    if (propertyInfo->type == Element::AnimationPropertyType::String) {
      return classified(AnimationTrackClass::StepOnly,
                        "string, enum, and choice properties require step-only frame evaluation");
    }

    if (m_interpolationMode == InterpolationMode::Step) {
      return classified(AnimationTrackClass::StepOnly,
                        "step interpolation requires discrete frame evaluation");
    }

    if ((qobject_cast<const SourceAsset*>(&target) ||
         qobject_cast<const ScriptedSurface*>(&target)) &&
        isDynamicProperty(target, m_propertyName)) {
      return classified(
        AnimationTrackClass::FrameBaked,
        "generated asset and scripted surface parameters can change generated topology");
    }

    if (isRuntimeContinuousTargetProperty(target, m_propertyName, propertyInfo->type)) {
      return classified(AnimationTrackClass::RuntimeContinuous,
                        "property can be sampled continuously by the render runtime");
    }

    return classified(AnimationTrackClass::FrameBaked,
                      "property changes render-scene state that must be baked per frame");
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

  render::animation::AnimationTrack AnimationTrack::toRenderTrack(const Element& target) const {
    const auto propertyInfo = target.animationPropertyInfo(m_propertyName);
    if (!propertyInfo)
      throw evaluationError(*this, "target property does not exist");

    if (!propertyInfo->writable)
      throw evaluationError(*this, "target property is not writable");

    switch (propertyInfo->type) {
    case Element::AnimationPropertyType::Double:
      return renderTrackFromKeys(*this, [](const auto& track, const auto& value) {
        return render::animation::AnimationValue(doubleFromJson(track, value));
      });
    case Element::AnimationPropertyType::Integer:
      return renderTrackFromKeys(*this, [](const auto& track, const auto& value) {
        return render::animation::AnimationValue(intFromJson(track, value));
      });
    case Element::AnimationPropertyType::Vector3:
      return renderTrackFromKeys(*this, [](const auto& track, const auto& value) {
        return render::animation::AnimationValue(vectorFromJson(track, value));
      });
    case Element::AnimationPropertyType::Color:
      return renderTrackFromKeys(*this, [](const auto& track, const auto& value) {
        return render::animation::AnimationValue(colorFromJson(track, value));
      });
    case Element::AnimationPropertyType::Boolean:
      return renderTrackFromKeys(*this, [](const auto& track, const auto& value) {
        return render::animation::AnimationValue(boolFromJson(track, value));
      });
    case Element::AnimationPropertyType::String:
      return renderTrackFromKeys(*this, [](const auto& track, const auto& value) {
        return render::animation::AnimationValue(stringFromJson(track, value).toStdString());
      });
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
