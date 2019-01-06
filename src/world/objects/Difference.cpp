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
      return make_named<raytracer::Difference>();
    } else {
      return nullptr;
    }
  } else {
    return make_named<raytracer::Composite>();
  }
}

static bool dummy = ElementFactory::self().registerClass<Difference>("Difference");

#include "Difference.moc"
