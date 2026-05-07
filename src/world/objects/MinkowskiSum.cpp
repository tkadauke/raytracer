#include "world/objects/ElementFactory.h"
#include "world/objects/MinkowskiSum.h"
#include "render/primitives/MinkowskiSum.h"
#include "render/primitives/ConvexHull.h"

MinkowskiSum::MinkowskiSum(Element* parent)
  : CSGSurface(parent)
{
}

std::shared_ptr<render::Primitive> MinkowskiSum::toRaytracerPrimitive() const {
  if (active()) {
    if (children().size() > 0) {
      return make_named<render::MinkowskiSum>();
    } else {
      return nullptr;
    }
  } else {
    return make_named<render::Composite>();
  }
}

static bool dummy = ElementFactory::self().registerClass<MinkowskiSum>("MinkowskiSum");

