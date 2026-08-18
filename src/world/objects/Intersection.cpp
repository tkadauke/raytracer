#include "world/objects/ElementFactory.h"
#include "world/objects/Intersection.h"
#include "render/primitives/Intersection.h"

Intersection::Intersection(Element* parent)
    : CSGSurface(parent) {
}

std::shared_ptr<render::Primitive> Intersection::toRaytracerPrimitive() const {
  return csgToRaytracerPrimitive<render::Intersection>();
}

static bool dummy = ElementFactory::self().registerClass<Intersection>("Intersection");
