#include "core/math/Vector.h"
#include "core/math/Angle.h"
#include "core/Color.h"
#include "world/objects/Element.h"

#include "world/objects/Material.h"
#include "world/objects/Texture.h"

#include "world/objects/ElementFactory.h"

#include <QMetaProperty>
#include <QVariant>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QUuid>

Q_DECLARE_METATYPE(Vector3d);
Q_DECLARE_METATYPE(Angled);
Q_DECLARE_METATYPE(Colord);

Element::Element(Element* parent)
  : QObject(parent),
    m_generated(false)
{
  m_id = QUuid::createUuid().toString();
}

Element::~Element() {
}

int Element::row() const {
  if (parent())
    return parent()->childElements().indexOf(const_cast<Element*>(this));

  return -1;
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
    if (i.key() == "type")
      continue;
    
    const char* name = i.key().toStdString().c_str();
    auto prop = property(name);

    QString type = QString(prop.typeName());

    auto value = i.value();

    if (!value.isUndefined()) {
      if (type == "Vector3d") {
        auto array = value.toArray();
        setProperty(name, QVariant::fromValue(Vector3d(array[0].toDouble(), array[1].toDouble(), array[2].toDouble())));
      } else if (type == "Angled") {
        auto angle = value.toDouble();
        setProperty(name, QVariant::fromValue(Angled::fromRadians(angle)));
      } else if (type == "Colord") {
        auto array = value.toArray();
        setProperty(name, QVariant::fromValue(Colord(array[0].toDouble(), array[1].toDouble(), array[2].toDouble())));
      } else if (i.key() != "id" && !QUuid(value.toString()).isNull()) {
        addPendingReference(name, value.toString());
      } else {
        setProperty(name, value.toVariant());
      }
    }
  }

  auto childElements = json["children"];
  if (childElements.isArray()) {
    for (const auto& child : childElements.toArray()) {
      auto type = child.toObject()["type"].toString().toStdString();
      Element* element = ElementFactory::self().create(type);
      if (element) {
        addChild(element);
        element->read(child.toObject());
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
  
  if (childElements().size() > 0) {
    QJsonArray childArray;
    for (const auto& child : childElements()) {
      Element* element = qobject_cast<Element*>(child);
      if (element && !element->isGenerated()) {
        QJsonObject elementObject;
        element->write(elementObject);
        childArray.append(elementObject);
      }
    }
    json["children"] = childArray;
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

  QString type = QString(prop.typeName());

  if (type == "Vector3d") {
    auto vector = prop.value<Vector3d>();
    json[name] = QJsonArray({ vector.x(), vector.y(), vector.z() });
  } else if (type == "Angled") {
    json[name] = prop.value<Angled>().radians();
  } else if (type == "Colord") {
    auto color = prop.value<Colord>();
    json[name] = QJsonArray({ color.r(), color.g(), color.b() });
  } else if (type == "QString") {
    json[name] = prop.toString();
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
    Element* value = elements[ref.second];
    if (qobject_cast<Material*>(value)) {
      variant = QVariant::fromValue<Material*>(static_cast<Material*>(value));
    } else if (qobject_cast<Texture*>(value)) {
      variant = QVariant::fromValue<Texture*>(static_cast<Texture*>(value));
    } else {
      std::cout << "Unable to resolve reference " << ref.first.toStdString() << ": " << ref.second.toStdString() << std::endl;
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

bool Element::canHaveChild(Element*) const {
  return false;
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

#include "Element.moc"
