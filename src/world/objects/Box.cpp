#include "world/objects/Box.h"
#include "raytracer/primitives/Box.h"
#include "raytracer/materials/MatteMaterial.h"

Box::Box(Element* parent)
  : Surface(parent)
{
}

raytracer::Primitive* Box::toRaytracerPrimitive() const {
  return new raytracer::Box(Vector3d::null(), size());
}

#include "Box.moc"
