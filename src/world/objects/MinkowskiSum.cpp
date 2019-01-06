#include "world/objects/ElementFactory.h"
#include "world/objects/MinkowskiSum.h"
#include "raytracer/primitives/MinkowskiSum.h"
#include "raytracer/primitives/ConvexHull.h"

MinkowskiSum::MinkowskiSum(Element* parent)
  : CSGSurface(parent)
{
}

std::shared_ptr<raytracer::Primitive> MinkowskiSum::toRaytracerPrimitive() const {
  if (active()) {
    if (children().size() > 0) {
      return make_named<raytracer::MinkowskiSum>();
    } else {
      return nullptr;
    }
  } else {
    return make_named<raytracer::Composite>();
  }
}

static bool dummy = ElementFactory::self().registerClass<MinkowskiSum>("MinkowskiSum");

#include "MinkowskiSum.moc"
