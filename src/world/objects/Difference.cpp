#include "world/objects/ElementFactory.h"
#include "world/objects/Difference.h"
#include "render/primitives/Difference.h"

Difference::Difference(Element* parent)
    : CSGSurface(parent) {
}

std::shared_ptr<render::Primitive> Difference::toRaytracerPrimitive() const {
  return csgToRaytracerPrimitive<render::Difference>();
}

static bool dummy = ElementFactory::self().registerClass<Difference>("Difference");
