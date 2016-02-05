#include "world/objects/Element.h"

Element::Element(Element* parent)
  : QObject(parent)
{
}

Element::~Element() {
}

#include "Element.moc"
