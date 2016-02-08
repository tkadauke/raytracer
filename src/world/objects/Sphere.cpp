#include "world/objects/Sphere.h"
#include "raytracer/primitives/Sphere.h"
#include "raytracer/materials/MatteMaterial.h"

Sphere::Sphere(Element* parent)
  : Surface(parent), m_radius(0)
{
}

raytracer::Primitive* Sphere::toRaytracerPrimitive() const {
  return new raytracer::Sphere(Vector3d::null(), radius());
}

#include "Sphere.moc"
