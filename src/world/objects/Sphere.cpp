#include "world/objects/Sphere.h"
#include "raytracer/primitives/Sphere.h"
#include "raytracer/materials/MatteMaterial.h"

Sphere::Sphere()
  : m_radius(0)
{
}

raytracer::Primitive* Sphere::toRaytracerPrimitive() const {
  raytracer::Sphere* result = new raytracer::Sphere(position(), radius());
  result->setMaterial(new raytracer::MatteMaterial(Colord(1, 0, 0)));
  return result;
}
