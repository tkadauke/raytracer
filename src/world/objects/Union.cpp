#include "world/objects/ElementFactory.h"
#include "world/objects/Union.h"
#include "render/primitives/Union.h"

Union::Union(Element* parent)
    : CSGSurface(parent) {
}

std::shared_ptr<render::Primitive> Union::toRaytracerPrimitive() const {
  if (active()) {
    if (children().size() > 0) {
      return make_named<render::Union>();
    } else {
      return nullptr;
    }
  } else {
    return make_named<render::Composite>();
  }
}

static bool dummy = ElementFactory::self().registerClass<Union>("Union");
