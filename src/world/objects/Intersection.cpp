#include "world/objects/ElementFactory.h"
#include "world/objects/Intersection.h"
#include "render/primitives/Intersection.h"

Intersection::Intersection(Element* parent)
    : CSGSurface(parent) {
}

std::shared_ptr<render::Primitive> Intersection::toRaytracerPrimitive() const {
  if (active()) {
    if (children().size() > 0) {
      return make_named<render::Intersection>();
    } else {
      return nullptr;
    }
  } else {
    return make_named<render::Composite>();
  }
}

static bool dummy = ElementFactory::self().registerClass<Intersection>("Intersection");
