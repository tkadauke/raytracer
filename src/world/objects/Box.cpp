#include "world/objects/ElementFactory.h"
#include "world/objects/Box.h"
#include "raytracer/primitives/Box.h"
#include "raytracer/materials/MatteMaterial.h"

Box::Box(Element* parent)
  : Surface(parent)
{
}

raytracer::Primitive* Box::toRaytracerPrimitive() const {
  return new raytracer::Box(Vector3d::null(), Vector3d::one());
}

static bool dummy = ElementFactory::self().registerClass<Box>("Box");

#include "Box.moc"
