#include "world/objects/MatteMaterial.h"

#include "raytracer/materials/MatteMaterial.h"

MatteMaterial::MatteMaterial(Element* parent)
  : Material(parent)
{
}

raytracer::Material* MatteMaterial::toRaytracerMaterial() const {
  return new raytracer::MatteMaterial(diffuseColor());
}

#include "MatteMaterial.moc"
