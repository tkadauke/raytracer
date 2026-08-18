#include "world/objects/ElementFactory.h"
#include "world/objects/MinkowskiSum.h"
#include "render/primitives/MinkowskiSum.h"
#include "render/primitives/ConvexHull.h"

MinkowskiSum::MinkowskiSum(Element* parent)
    : CSGSurface(parent) {
}

std::shared_ptr<render::Primitive> MinkowskiSum::toRaytracerPrimitive() const {
  return csgToRaytracerPrimitive<render::MinkowskiSum>();
}

static bool dummy = ElementFactory::self().registerClass<MinkowskiSum>("MinkowskiSum");
