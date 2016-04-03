#include "world/objects/ElementFactory.h"
#include "world/objects/ConvexHull.h"
#include "raytracer/primitives/ConvexHull.h"
#include "raytracer/primitives/ConvexHull.h"

ConvexHull::ConvexHull(Element* parent)
  : CSGSurface(parent)
{
}

std::shared_ptr<raytracer::Primitive> ConvexHull::toRaytracerPrimitive() const {
  if (active()) {
    if (children().size() > 0) {
      return std::make_shared<raytracer::ConvexHull>();
    } else {
      return nullptr;
    }
  } else {
    return std::make_shared<raytracer::Composite>();
  }
}

static bool dummy = ElementFactory::self().registerClass<ConvexHull>("ConvexHull");

#include "ConvexHull.moc"
