#include "world/objects/ElementFactory.h"
#include "world/objects/Group.h"
#include "world/objects/Light.h"
#include "world/objects/Surface.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Instance.h"
#include "render/primitives/Scene.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
  std::optional<int> metadataInt(const QJsonObject& metadata, const QString& key) {
    const auto value = metadata.value(key);
    if (!value.isDouble())
      return std::nullopt;

    const auto number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number)
      return std::nullopt;
    if (number < std::numeric_limits<int>::min() ||
        number > std::numeric_limits<int>::max())
      return std::nullopt;

    return static_cast<int>(number);
  }

  std::optional<double> metadataDouble(const QJsonObject& metadata, const QString& key) {
    const auto value = metadata.value(key);
    if (!value.isDouble())
      return std::nullopt;

    return value.toDouble();
  }

  std::optional<QString> metadataString(const QJsonObject& metadata, const QString& key) {
    const auto value = metadata.value(key);
    if (!value.isString())
      return std::nullopt;

    return value.toString();
  }
}

Group::Group(Element* parent)
    : Transformable(parent),
      m_visible(true) {
}

void Group::setMetadataValue(const QString& key, const QJsonValue& value) {
  if (value.isUndefined()) {
    m_metadata.remove(key);
  } else {
    m_metadata.insert(key, value);
  }
}

std::optional<int> Group::stepIndex() const {
  return metadataInt(m_metadata, GroupMetadata::stepIndexKey());
}

void Group::setStepIndex(std::optional<int> index) {
  if (index) {
    setMetadataValue(GroupMetadata::stepIndexKey(), *index);
  } else {
    setMetadataValue(GroupMetadata::stepIndexKey(), QJsonValue::Undefined);
  }
}

std::optional<int> Group::layerIndex() const {
  return metadataInt(m_metadata, GroupMetadata::layerIndexKey());
}

void Group::setLayerIndex(std::optional<int> index) {
  if (index) {
    setMetadataValue(GroupMetadata::layerIndexKey(), *index);
  } else {
    setMetadataValue(GroupMetadata::layerIndexKey(), QJsonValue::Undefined);
  }
}

std::optional<double> Group::startTime() const {
  return metadataDouble(m_metadata, GroupMetadata::startTimeKey());
}

std::optional<double> Group::endTime() const {
  return metadataDouble(m_metadata, GroupMetadata::endTimeKey());
}

void Group::setTimeRange(std::optional<double> startTime,
                         std::optional<double> endTime) {
  if (startTime) {
    setMetadataValue(GroupMetadata::startTimeKey(), *startTime);
  } else {
    setMetadataValue(GroupMetadata::startTimeKey(), QJsonValue::Undefined);
  }

  if (endTime) {
    setMetadataValue(GroupMetadata::endTimeKey(), *endTime);
  } else {
    setMetadataValue(GroupMetadata::endTimeKey(), QJsonValue::Undefined);
  }
}

std::optional<QString> Group::label() const {
  return metadataString(m_metadata, GroupMetadata::labelKey());
}

void Group::setLabel(const std::optional<QString>& label) {
  if (label) {
    setMetadataValue(GroupMetadata::labelKey(), *label);
  } else {
    setMetadataValue(GroupMetadata::labelKey(), QJsonValue::Undefined);
  }
}

void Group::read(const QJsonObject& json) {
  auto groupJson = json;
  groupJson.remove("metadata");

  Transformable::read(groupJson);

  const auto metadataValue = json["metadata"];
  if (metadataValue.isUndefined()) {
    m_metadata = QJsonObject();
    return;
  }

  if (!metadataValue.isObject())
    throw std::invalid_argument("group metadata must be an object");

  m_metadata = metadataValue.toObject();
}

void Group::write(QJsonObject& json) {
  Transformable::write(json);

  if (!m_metadata.isEmpty()) {
    json["metadata"] = m_metadata;
  }
}

std::shared_ptr<render::Primitive>
Group::applyTransform(std::shared_ptr<render::Primitive> primitive) const {
  auto result = std::make_shared<render::Instance>(primitive);
  result->setMatrix(localTransform());
  return result;
}

std::shared_ptr<render::Primitive> Group::toRaytracer(render::Scene* scene) const {
  if (!visible())
    return nullptr;

  auto composite = make_named<render::Composite>();

  for (const auto& child : childElements()) {
    if (auto surface = qobject_cast<Surface*>(child)) {
      auto primitive = surface->toRaytracer(scene);
      if (primitive)
        composite->add(primitive);
    } else if (auto group = qobject_cast<Group*>(child)) {
      auto primitive = group->toRaytracer(scene);
      if (primitive)
        composite->add(primitive);
    } else if (auto light = qobject_cast<Light*>(child)) {
      if (light->visible())
        scene->addLight(light->toRaytracer());
    }
  }

  if (composite->primitives().empty())
    return nullptr;

  return applyTransform(composite);
}

bool Group::canHaveChild(Element* child) const {
  return dynamic_cast<Surface*>(child) != nullptr || dynamic_cast<Light*>(child) != nullptr ||
         dynamic_cast<Group*>(child) != nullptr;
}

static bool dummy = ElementFactory::self().registerClass<Group>("Group") &&
                    ElementFactory::self().registerClass<Group>("Collection");
