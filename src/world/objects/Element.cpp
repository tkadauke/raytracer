#include "core/math/Vector.h"
#include "core/math/Angle.h"
#include "core/Color.h"
#include "world/objects/Element.h"

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
  : QObject(parent)
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
  auto klass = metaObject();
  
  for (int i = 0; i != klass->propertyCount(); ++i) {
    auto metaProp = klass->property(i);
    auto prop = property(metaProp.name());

    QString type = QString(metaProp.typeName());

    auto value = json[metaProp.name()];

    if (!value.isUndefined()) {
      if (type == "Vector3d") {
        auto array = value.toArray();
        setProperty(metaProp.name(), QVariant::fromValue(Vector3d(array[0].toDouble(), array[1].toDouble(), array[2].toDouble())));
      } else if (type == "Angled") {
        auto angle = value.toDouble();
        setProperty(metaProp.name(), QVariant::fromValue(Angled::fromRadians(angle)));
      } else if (type == "Colord") {
        auto array = value.toArray();
        setProperty(metaProp.name(), QVariant::fromValue(Colord(array[0].toDouble(), array[1].toDouble(), array[2].toDouble())));
      } else if (type == "Material*" || type == "Texture*") {
        addPendingReference(metaProp.name(), value.toString());
      } else {
        setProperty(metaProp.name(), value.toVariant());
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
  
  if (childElements().size() > 0) {
    QJsonArray childArray;
    for (const auto& child : childElements()) {
      Element* element = qobject_cast<Element*>(child);
      if (element) {
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
    auto prop = property(metaProp.name());

    QString type = QString(metaProp.typeName());

    if (type == "Vector3d") {
      auto vector = prop.value<Vector3d>();
      json[metaProp.name()] = QJsonArray({ vector.x(), vector.y(), vector.z() });
    } else if (type == "Angled") {
      json[metaProp.name()] = prop.value<Angled>().radians();
    } else if (type == "Colord") {
      auto color = prop.value<Colord>();
      json[metaProp.name()] = QJsonArray({ color.r(), color.g(), color.b() });
    } else if (type == "QString") {
      json[metaProp.name()] = prop.toString();
    } else if (type == "double") {
      json[metaProp.name()] = prop.toDouble();
    } else if (type == "bool") {
      json[metaProp.name()] = prop.toBool();
    } else if (type == "Material*" || type == "Texture*") {
      auto element = prop.value<Element*>();
      if (element) {
        json[metaProp.name()] = element->id();
      }
    }
  }
}

void Element::addPendingReference(const QString& property, const QString& id) {
  m_pendingReferences << QPair<QString, QString>(property, id);
}

void Element::resolveReferences(const QMap<QString, Element*>& elements) {
  for (const auto& ref : m_pendingReferences) {
    setProperty(ref.first.toStdString().c_str(), QVariant::fromValue(elements[ref.second]));
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
