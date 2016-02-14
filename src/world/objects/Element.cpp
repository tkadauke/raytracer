#include "world/objects/Element.h"

#include <QMetaProperty>
#include <QVariant>

Element::Element(Element* parent)
  : QObject(parent)
{
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

#include "Element.moc"
