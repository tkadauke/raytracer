#include "world/objects/Sphere.h"
#include "raytracer/primitives/Sphere.h"
#include "raytracer/materials/MatteMaterial.h"

Sphere::Sphere(Element* parent)
  : Surface(parent)
{
}

raytracer::Primitive* Sphere::toRaytracerPrimitive() const {
  return new raytracer::Sphere(Vector3d::null(), 1);
}

#include "Sphere.moc"
