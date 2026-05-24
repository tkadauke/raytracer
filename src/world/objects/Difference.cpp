#include "world/objects/ElementFactory.h"
#include "world/objects/Difference.h"
#include "render/primitives/Difference.h"

Difference::Difference(Element* parent)
    : CSGSurface(parent) {
}

std::shared_ptr<render::Primitive> Difference::toRaytracerPrimitive() const {
  if (active()) {
    if (children().size() > 0) {
      return make_named<render::Difference>();
    } else {
      return nullptr;
    }
  } else {
    return make_named<render::Composite>();
  }
}

static bool dummy = ElementFactory::self().registerClass<Difference>("Difference");
