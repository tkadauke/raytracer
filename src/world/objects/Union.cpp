#include "world/objects/ElementFactory.h"
#include "world/objects/Union.h"
#include "render/primitives/Union.h"

Union::Union(Element* parent)
    : CSGSurface(parent) {
}

std::shared_ptr<render::Primitive> Union::toRaytracerPrimitive() const {
  return csgToRaytracerPrimitive<render::Union>();
}

static bool dummy = ElementFactory::self().registerClass<Union>("Union");
