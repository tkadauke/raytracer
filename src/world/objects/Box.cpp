#include "world/objects/Box.h"
#include "raytracer/primitives/Box.h"
#include "raytracer/materials/MatteMaterial.h"

Box::Box(Element* parent)
  : Surface(parent)
{
}

raytracer::Primitive* Box::toRaytracerPrimitive() const {
  raytracer::Box* result = new raytracer::Box(Vector3d::null(), size());
  result->setMaterial(new raytracer::MatteMaterial(Colord(1, 0, 0)));
  return applyTransform(result);
}

#include "Box.moc"
