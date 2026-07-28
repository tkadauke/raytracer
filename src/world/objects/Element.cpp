#include "core/math/Vector.h"
#include "core/math/Angle.h"
#include "core/Color.h"
#include "core/json/JsonValue.h"
#include "engine/graph/RenderSceneAnalysis.h"
#include "render/Object.h"
#include "world/objects/Element.h"

#include "world/objects/Material.h"
#include "world/objects/Scene.h"
#include "world/objects/Texture.h"

#include "world/objects/ElementFactory.h"

#include <QMetaProperty>
#include <QRegularExpression>
#include <QVariant>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QUuid>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

Q_DECLARE_METATYPE(Vector3d);
Q_DECLARE_METATYPE(Angled);
Q_DECLARE_METATYPE(Colord);

Element::Element(Element* parent)
    : QObject(parent),
      m_generated(false) {
  m_id = QUuid::createUuid().toString();
}

Element::~Element() {
}

bool Element::displayInSceneModel() const {
  return !isGenerated();
}

bool Element::isPropertyVisible(const QString&) const {
  return true;
}

QString Element::propertyDisplayName(const QString& propertyName) const {
  return humanizePropertyName(propertyName);
}

QString Element::propertyDescription(const QString&) const {
  return {};
}

QString Element::propertyGroup(const QString&) const {
  return QStringLiteral("Properties");
}

QStringList Element::propertyChoices(const QString&) const {
  return {};
}

QList<int> Element::propertyIntChoices(const QString&) const {
  return {};
}

std::optional<QPair<int, int>> Element::propertyIntRange(const QString&) const {
  return std::nullopt;
}

std::optional<QPair<double, double>> Element::propertyDoubleRange(const QString&) const {
  return std::nullopt;
}

std::optional<double> Element::propertyDoubleStep(const QString&) const {
  return std::nullopt;
}

QString Element::propertyChoiceDisplayName(const QString&, const QString& choice) const {
  return humanizePropertyName(choice);
}

bool Element::rebuildPropertyEditorAfterChange(const QString&) const {
  return false;
}

void Element::propertyEdited(const QString&) {
}

std::optional<Element::AnimationPropertyInfo>
Element::animationPropertyInfo(const QString& propertyName) const {
  const int propertyIndex = metaObject()->indexOfProperty(propertyName.toLatin1().constData());
  if (propertyIndex < 0)
    return std::nullopt;

  const auto property = metaObject()->property(propertyIndex);
  const QString typeName = property.typeName();
  return AnimationPropertyInfo{animationPropertyTypeForTypeName(typeName), typeName,
                               property.isWritable()};
}

bool Element::setAnimatedProperty(const QString& propertyName, const QVariant& value) {
  const bool applied = setProperty(propertyName.toLatin1().constData(), value);
  if (applied)
    propertyEdited(propertyName);
  return applied;
}

Element::AnimationPropertyType
Element::animationPropertyTypeForTypeName(const QString& typeName) const {
  if (typeName == QStringLiteral("double"))
    return AnimationPropertyType::Double;
  if (typeName == QStringLiteral("int"))
    return AnimationPropertyType::Integer;
  if (typeName == QStringLiteral("Vector3<double>"))
    return AnimationPropertyType::Vector3;
  if (typeName == QStringLiteral("Color<double>"))
    return AnimationPropertyType::Color;
  if (typeName == QStringLiteral("bool"))
    return AnimationPropertyType::Boolean;

  return AnimationPropertyType::Unsupported;
}

QString Element::humanizePropertyName(const QString& propertyName) const {
  QString text = propertyName;
  text.replace(QChar('_'), QChar(' '));
  text.replace(QChar('-'), QChar(' '));
  text.replace(QRegularExpression(QStringLiteral("([a-z0-9])([A-Z])")), QStringLiteral("\\1 \\2"));

  const auto words = text.split(QChar(' '), Qt::SkipEmptyParts);
  QStringList titleWords;
  for (QString word : words) {
    if (word.size() <= 3 && word == word.toUpper()) {
      titleWords << word;
      continue;
    }
    word = word.toLower();
    word[0] = word[0].toUpper();
    titleWords << word;
  }
  return titleWords.join(QChar(' '));
}

int Element::row() const {
  if (parent())
    return parent()->childElements().indexOf(const_cast<Element*>(this));

  return -1;
}

namespace {
  std::vector<std::string> toStdStrings(const std::vector<QString>& values) {
    std::vector<std::string> result;
    result.reserve(values.size());
    for (const auto& value : values) {
      result.push_back(value.toStdString());
    }
    return result;
  }

  void appendMetadataString(std::vector<QString>& result, const QJsonValue& value) {
    if (value.isString()) {
      const QString text = value.toString().trimmed();
      if (!text.isEmpty()) {
        result.push_back(text);
      }
    }
  }

  void appendMetadataStringArray(std::vector<QString>& result, const QJsonValue& value) {
    if (value.isArray()) {
      for (const auto& entry : value.toArray()) {
        appendMetadataString(result, entry);
      }
    } else {
      appendMetadataString(result, value);
    }
  }
}

std::vector<QString> Element::renderGraphTags() const {
  std::vector<QString> result;
  appendMetadataStringArray(result, metadataValue(QStringLiteral("tags")));
  appendMetadataString(result, metadataValue(QStringLiteral("tag")));
  return result;
}

std::vector<QString> Element::renderGraphLayers() const {
  std::vector<QString> result;
  appendMetadataStringArray(result, metadataValue(QStringLiteral("layers")));
  appendMetadataString(result, metadataValue(QStringLiteral("layer")));
  appendMetadataString(result, metadataValue(QStringLiteral("layerName")));

  const auto layerIndex = metadataValue(QStringLiteral("layerIndex"));
  if (layerIndex.isDouble()) {
    result.push_back(QString::number(layerIndex.toInt()));
  }
  return result;
}

void Element::unlink(Element* root) {
  for (int i = 0; i != root->metaObject()->propertyCount(); ++i) {
    auto metaProp = root->metaObject()->property(i);
    auto prop = root->property(metaProp.name());

    if (prop.value<Element*>() == this) {
      root->setProperty(metaProp.name(), QVariant::fromValue<Element*>(nullptr));
    }
  }

  for (const auto& child : root->childElements()) {
    unlink(child);
  }
}

void Element::read(const QJsonObject& json) {
  for (auto i = json.begin(); i != json.end(); ++i) {
    if (i.key() == "type" || i.key() == "metadata")
      continue;

    auto propertyName = i.key();
    auto propertyNameStdString = propertyName.toStdString();
    auto propertyNameCStr = propertyNameStdString.c_str();
    auto prop = property(propertyNameCStr);

    QString type = QString(prop.typeName());

    auto value = i.value();

    if (!value.isUndefined()) {
      if (type == "Vector3<double>") {
        auto array = value.toArray();
        setProperty(propertyNameCStr, QVariant::fromValue(core::json::vector3FromJsonArray(array)));
      } else if (type == "Angle<double>") {
        auto angle = value.toDouble();
        setProperty(propertyNameCStr, QVariant::fromValue(Angled::fromRadians(angle)));
      } else if (type == "Color<double>") {
        auto array = value.toArray();
        setProperty(propertyNameCStr, QVariant::fromValue(core::json::colorFromJsonArray(array)));
      } else if (propertyName != "id" &&
                 (type.endsWith("*") || !QUuid(value.toString()).isNull())) {
        // JSON `null` is a valid way to say "this reference is
        // intentionally unset" — `world::Surface` writes its
        // `material` as `null` when no material is attached. Skip
        // the pending-reference machinery in that case so
        // `resolveReferences` doesn't print a spurious "unable to
        // resolve" line for an empty-string lookup.
        if (!value.isNull()) {
          addPendingReference(propertyName, value.toString());
        }
      } else {
        setProperty(propertyNameCStr, value.toVariant());
      }
    }
  }

  const auto metadataValue = json["metadata"];
  if (metadataValue.isUndefined()) {
    m_metadata = QJsonObject();
  } else {
    if (!metadataValue.isObject())
      throw std::invalid_argument("element metadata must be an object");

    m_metadata = metadataValue.toObject();
  }

  auto childElements = json["children"];
  if (childElements.isArray()) {
    for (const auto& child : childElements.toArray()) {
      auto type = child.toObject()["type"].toString().toStdString();
      auto element = ElementFactory::self().create(type);
      if (element) {
        Element* raw = element.get();
        addChild(std::move(element));
        raw->read(child.toObject());
      } else {
        qWarning("Unknown element type %s", type.c_str());
      }
    }
  }
}

void Element::write(QJsonObject& json) {
  json["type"] = metaObject()->className();

  writeForClass(metaObject(), json);

  for (const auto& name : dynamicPropertyNames()) {
    writeProperty(name, json);
  }

  if (!m_metadata.isEmpty()) {
    json["metadata"] = m_metadata;
  }

  QJsonArray childArray;
  for (const auto& child : childElements()) {
    Element* element = qobject_cast<Element*>(child);
    if (element && !element->isGenerated()) {
      QJsonObject elementObject;
      element->write(elementObject);
      childArray.append(elementObject);
    }
  }
  if (!childArray.isEmpty()) {
    json["children"] = childArray;
  }
}

void Element::setMetadataValue(const QString& key, const QJsonValue& value) {
  if (value.isUndefined()) {
    m_metadata.remove(key);
  } else {
    m_metadata.insert(key, value);
  }
}

void Element::writeForClass(const QMetaObject* klass, QJsonObject& json) {
  if (klass->className() != QString("Element") && klass->superClass()) {
    writeForClass(klass->superClass(), json);
  }

  for (int i = klass->propertyOffset(); i != klass->propertyCount(); ++i) {
    auto metaProp = klass->property(i);
    writeProperty(metaProp.name(), json);
  }
}

void Element::writeProperty(const QString& name, QJsonObject& json) {
  auto prop = property(name.toStdString().c_str());

  // Qt 6 `QVariant::typeName()` reports the underlying templated name
  // (`Vector3<double>`, `Color<double>`, `Angle<double>`) instead of
  // the `Vector3d` / `Colord` / `Angled` typedef Qt 5 used. The
  // typedef alias never went through `qRegisterMetaType`, so the
  // resolved name is the canonical template instantiation.
  QString type = QString(prop.typeName());

  if (type == "Vector3<double>") {
    auto vector = prop.value<Vector3d>();
    json[name] = core::json::vector3ToJsonArray(vector);
  } else if (type == "Angle<double>") {
    json[name] = prop.value<Angled>().radians();
  } else if (type == "Color<double>") {
    auto color = prop.value<Colord>();
    json[name] = core::json::colorToJsonArray(color);
  } else if (type == "QString") {
    json[name] = prop.toString();
  } else if (type == "int") {
    json[name] = prop.toInt();
  } else if (type == "double") {
    json[name] = prop.toDouble();
  } else if (type == "bool") {
    json[name] = prop.toBool();
  } else if (type == "Material*" || type == "Texture*") {
    auto element = prop.value<Element*>();
    if (element) {
      json[name] = element->id();
    }
  }
}

void Element::addPendingReference(const QString& property, const QString& id) {
  m_pendingReferences << QPair<QString, QString>(property, id);
}

void Element::resolveReferences(const QMap<QString, Element*>& elements) {
  for (const auto& ref : m_pendingReferences) {
    QVariant variant;
    Element* value = elements.value(ref.second, nullptr);
    if (qobject_cast<Material*>(value)) {
      variant = QVariant::fromValue<Material*>(static_cast<Material*>(value));
    } else if (qobject_cast<Texture*>(value)) {
      variant = QVariant::fromValue<Texture*>(static_cast<Texture*>(value));
    } else {
      std::cout << "Unable to resolve reference " << ref.first.toStdString() << ": "
                << ref.second.toStdString() << std::endl;
    }
    setProperty(ref.first.toStdString().c_str(), variant);
  }
  m_pendingReferences.clear();

  for (const auto& child : childElements()) {
    child->resolveReferences(elements);
  }
}

void Element::leaveParent() {
}

void Element::joinParent() {
}

Element* Element::findById(const QString& id) {
  if (id == this->id()) {
    return this;
  } else {
    for (const auto& child : childElements()) {
      auto result = child->findById(id);
      if (result)
        return result;
    }
  }
  return nullptr;
}

const Element* Element::findById(const QString& id) const {
  if (id == this->id()) {
    return this;
  } else {
    for (const auto& child : childElements()) {
      auto result = child->findById(id);
      if (result)
        return result;
    }
  }
  return nullptr;
}

bool Element::canHaveChild(Element*) const {
  return false;
}

void Element::contributeToRenderGraphAnalysis(engine::graph::RenderSceneAnalysis& analysis) const {
  for (const auto& child : childElements()) {
    child->contributeToRenderGraphAnalysis(analysis);
  }
}

void Element::contributeSelectableObjectToRenderGraphAnalysis(
  engine::graph::RenderSceneAnalysis& analysis) const {
  analysis.recordSelectableObject(id().toStdString(), name().toStdString(),
                                  toStdStrings(renderGraphTags()),
                                  toStdStrings(renderGraphLayers()),
                                  displayName().toStdString());
}

void Element::attachRuntimeAnimationTracks(render::Object& object) const {
  const auto* root = this;
  while (root->parent()) {
    root = root->parent();
  }

  const auto* scene = qobject_cast<const Scene*>(root);
  if (!scene || !scene->animation())
    return;

  object.setMetadataValue("world:id", id().toStdString());
  const int frame = scene->evaluatedAnimationFrame().value_or(scene->animation()->startFrame());
  object.setMetadataValue("animation:evaluatedFrame", std::to_string(frame));
  for (const auto& track : scene->animation()->tracks()) {
    if (track.targetId() != id())
      continue;

    const auto classification = track.classify(*this);
    if (classification.trackClass != world::AnimationTrackClass::RuntimeContinuous)
      continue;

    object.setAnimationTrack(track.propertyName().toStdString(), track.toRenderTrack(*this));
  }
}

void Element::insertChild(int index, Element* child) {
  Element* p = child->parent();
  if (p) {
    child->leaveParent();
    p->removeChild(p->childElements().indexOf(child));
  }

  m_childElements.insert(index, child);
  child->setParent(this);
  child->joinParent();
}

void Element::removeChild(int index, bool removeParent) {
  auto child = m_childElements[index];
  if (removeParent) {
    child->setParent(nullptr);
  }
  m_childElements.removeAt(index);
}

void Element::moveChild(int from, int to) {
  if (to == from || to - 1 == from) {
    return;
  }

  if (to > from) {
    if (to - 1 != from) {
      m_childElements.move(from, to - 1);
    }
  } else {
    m_childElements.move(from, to);
  }
}
