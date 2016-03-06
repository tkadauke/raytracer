#include "world/objects/ElementFactory.h"
#include "world/objects/Union.h"
#include "raytracer/primitives/Union.h"

Union::Union(Element* parent)
  : CSGSurface(parent)
{
}

std::shared_ptr<raytracer::Primitive> Union::toRaytracerPrimitive() const {
  if (active()) {
    if (children().size() > 0) {
      return std::make_shared<raytracer::Union>();
    } else {
      return nullptr;
    }
  } else {
    return std::make_shared<raytracer::Composite>();
  }
}

static bool dummy = ElementFactory::self().registerClass<Union>("Union");

#include "Union.moc"
