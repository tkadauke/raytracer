#include "world/objects/Element.h"

Element::Element(Element* parent)
  : QObject(parent)
{
}

Element::~Element() {
}

int Element::row() const
{
  if (parent())
    return parent()->children().indexOf(const_cast<Element*>(this));

  return 0;
}

#include "Element.moc"
