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
      return make_named<raytracer::ConvexHull>();
    } else {
      return nullptr;
    }
  } else {
    return make_named<raytracer::Composite>();
  }
}

static bool dummy = ElementFactory::self().registerClass<ConvexHull>("ConvexHull");

#include "ConvexHull.moc"
