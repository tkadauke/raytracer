#include "world/objects/ElementFactory.h"
#include "world/objects/Difference.h"
#include "raytracer/primitives/Difference.h"

Difference::Difference(Element* parent)
  : CSGSurface(parent)
{
}

std::shared_ptr<raytracer::Primitive> Difference::toRaytracerPrimitive() const {
  if (active()) {
    if (children().size() > 0) {
      return std::make_shared<raytracer::Difference>();
    } else {
      return nullptr;
    }
  } else {
    return std::make_shared<raytracer::Composite>();
  }
}

static bool dummy = ElementFactory::self().registerClass<Difference>("Difference");

#include "Difference.moc"
