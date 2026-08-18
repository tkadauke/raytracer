#include "world/objects/ElementFactory.h"
#include "world/objects/ConvexHull.h"
#include "render/primitives/ConvexHull.h"

ConvexHull::ConvexHull(Element* parent)
    : CSGSurface(parent) {
}

std::shared_ptr<render::Primitive> ConvexHull::toRaytracerPrimitive() const {
  return csgToRaytracerPrimitive<render::ConvexHull>();
}

static bool dummy = ElementFactory::self().registerClass<ConvexHull>("ConvexHull");
