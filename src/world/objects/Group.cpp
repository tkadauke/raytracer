#include "world/objects/ElementFactory.h"
#include "world/objects/Group.h"
#include "world/objects/Light.h"
#include "world/objects/StepVisibilityEvaluator.h"
#include "world/objects/Surface.h"
#include "render/materials/MatteMaterial.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Instance.h"
#include "render/primitives/Scene.h"

#include <cmath>
#include <limits>

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

  std::shared_ptr<render::Material> stepPlaybackMaterial(const Colord& color) {
    auto material =
      std::make_shared<render::MatteMaterial>(std::make_shared<render::ConstantColorTexture>(color));
    material->setAmbientCoefficient(1.0);
    material->setDiffuseCoefficient(0.65);
    return material;
  }

  StepVisibilitySelection stepPlaybackSelection(const StepPlaybackStyle& style) {
    if (!style.enabled())
      return StepVisibilitySelection::all();
    if (style.activeTime && !style.activeStep)
      return StepVisibilitySelection::atTime(*style.activeTime);
    if (style.ghostPrevious)
      return StepVisibilitySelection::cumulativeThrough(*style.activeStep);
    return StepVisibilitySelection::onlyStep(*style.activeStep);
  }
}

Group::Group(Element* parent)
    : Transformable(parent),
      m_visible(true) {
}

std::optional<int> Group::stepIndex() const {
  return metadataInt(metadata(), GroupMetadata::stepIndexKey());
}

void Group::setStepIndex(std::optional<int> index) {
  if (index) {
    setMetadataValue(GroupMetadata::stepIndexKey(), *index);
  } else {
    setMetadataValue(GroupMetadata::stepIndexKey(), QJsonValue::Undefined);
  }
}

std::optional<int> Group::layerIndex() const {
  return metadataInt(metadata(), GroupMetadata::layerIndexKey());
}

void Group::setLayerIndex(std::optional<int> index) {
  if (index) {
    setMetadataValue(GroupMetadata::layerIndexKey(), *index);
  } else {
    setMetadataValue(GroupMetadata::layerIndexKey(), QJsonValue::Undefined);
  }
}

std::optional<double> Group::startTime() const {
  return metadataDouble(metadata(), GroupMetadata::startTimeKey());
}

std::optional<double> Group::endTime() const {
  return metadataDouble(metadata(), GroupMetadata::endTimeKey());
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
  return metadataString(metadata(), GroupMetadata::labelKey());
}

void Group::setLabel(const std::optional<QString>& label) {
  if (label) {
    setMetadataValue(GroupMetadata::labelKey(), *label);
  } else {
    setMetadataValue(GroupMetadata::labelKey(), QJsonValue::Undefined);
  }
}

std::shared_ptr<render::Primitive>
Group::applyTransform(std::shared_ptr<render::Primitive> primitive) const {
  auto result = std::make_shared<render::Instance>(primitive);
  result->setMatrix(localTransform());
  return result;
}

std::shared_ptr<render::Primitive> Group::toRaytracer(render::Scene* scene) const {
  return toRaytracer(scene, StepPlaybackStyle());
}

std::shared_ptr<render::Primitive> Group::toRaytracer(render::Scene* scene,
                                                     const StepPlaybackStyle& style) const {
  const StepVisibilityEvaluator evaluator(stepPlaybackSelection(style));
  const StepVisualRole role = evaluator.visualRole(*this, style);
  if (role == StepVisualRole::Hidden)
    return nullptr;

  if (!visible())
    return nullptr;

  auto composite = make_named<render::Composite>();

  for (const auto& child : childElements()) {
    if (auto surface = qobject_cast<Surface*>(child)) {
      auto primitive = surface->toRaytracer(scene, style);
      if (primitive)
        composite->add(primitive);
    } else if (auto group = qobject_cast<Group*>(child)) {
      auto primitive = group->toRaytracer(scene, style);
      if (primitive)
        composite->add(primitive);
    } else if (auto light = qobject_cast<Light*>(child)) {
      if (light->visible())
        scene->addLight(light->toRaytracer());
    }
  }

  if (composite->primitives().empty())
    return nullptr;

  auto result = applyTransform(composite);
  if (role == StepVisualRole::Active)
    result->setMaterial(stepPlaybackMaterial(style.activeColor));
  else if (role == StepVisualRole::Previous)
    result->setMaterial(stepPlaybackMaterial(style.ghostColor));
  return result;
}

bool Group::canHaveChild(Element* child) const {
  return dynamic_cast<Surface*>(child) != nullptr || dynamic_cast<Light*>(child) != nullptr ||
         dynamic_cast<Group*>(child) != nullptr;
}

static bool dummy = ElementFactory::self().registerClass<Group>("Group") &&
                    ElementFactory::self().registerClass<Group>("Collection");
