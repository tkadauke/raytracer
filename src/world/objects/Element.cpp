#include "core/math/Vector.h"
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
Q_DECLARE_METATYPE(Colord);

Element::Element(Element* parent)
  : QObject(parent)
{
  m_id = QUuid::createUuid().toString();
}

Element::~Element() {
  QObject* e = this;
  while (e) {
    if (e->parent() && e->parent()->inherits("Element")) {
      e = e->parent();
    } else {
      break;
    }
  }
  
  unlink(e);
}

int Element::row() const
{
  if (parent())
    return parent()->children().indexOf(const_cast<Element*>(this));

  return 0;
}

void Element::unlink(QObject* root) {
  for (int i = 0; i != root->metaObject()->propertyCount(); ++i) {
    auto metaProp = root->metaObject()->property(i);
    auto prop = root->property(metaProp.name());
    
    if (prop.value<QObject*>() == this) {
      root->setProperty(metaProp.name(), QVariant::fromValue<QObject*>(nullptr));
    }
  }
  
  for (const auto& child : root->children()) {
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

    if (!value.isNull()) {
      if (type == "Vector3d") {
        auto array = value.toArray();
        setProperty(metaProp.name(), QVariant::fromValue(Vector3d(array[0].toDouble(), array[1].toDouble(), array[2].toDouble())));
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
  
  auto children = json["children"];
  if (children.isArray()) {
    for (const auto& child : children.toArray()) {
      Element* element = ElementFactory::self().create(child.toObject()["type"].toString().toStdString());
      element->setParent(this);
      element->read(child.toObject());
    }
  }
}

void Element::write(QJsonObject& json) {
  json["type"] = metaObject()->className();
  
  writeForClass(metaObject(), json);
  
  if (children().size() > 0) {
    QJsonArray childArray;
    for (const auto& child : children()) {
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
  
  for (const auto& child : children()) {
    Element* e = qobject_cast<Element*>(child);
    if (e) {
      e->resolveReferences(elements);
    }
  }
}

#include "Element.moc"
