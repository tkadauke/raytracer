#include "world/objects/ElementFactory.h"
#include "world/objects/ConvexHull.h"
#include "render/primitives/ConvexHull.h"
#include "render/primitives/ConvexHull.h"

ConvexHull::ConvexHull(Element* parent)
    : CSGSurface(parent) {
}

std::shared_ptr<render::Primitive> ConvexHull::toRaytracerPrimitive() const {
  if (active()) {
    if (children().size() > 0) {
      return make_named<render::ConvexHull>();
    } else {
      return nullptr;
    }
  } else {
    return make_named<render::Composite>();
  }
}

static bool dummy = ElementFactory::self().registerClass<ConvexHull>("ConvexHull");
