#include "world/objects/ElementFactory.h"
#include "world/objects/Intersection.h"
#include "raytracer/primitives/Intersection.h"
#include "raytracer/materials/MatteMaterial.h"

Intersection::Intersection(Element* parent)
  : Surface(parent), m_active(true)
{
}

std::shared_ptr<raytracer::Primitive> Intersection::toRaytracerPrimitive() const {
  if (active()) {
    if (children().size() > 0) {
      return std::make_shared<raytracer::Intersection>();
    } else {
      return nullptr;
    }
  } else {
    return std::make_shared<raytracer::Composite>();
  }
}

static bool dummy = ElementFactory::self().registerClass<Intersection>("Intersection");

#include "Intersection.moc"
