#include "world/objects/ElementFactory.h"
#include "world/objects/Sphere.h"
#include "raytracer/primitives/Sphere.h"
#include "raytracer/materials/MatteMaterial.h"

Sphere::Sphere(Element* parent)
  : Surface(parent)
{
}

std::shared_ptr<raytracer::Primitive> Sphere::toRaytracerPrimitive() const {
  return std::make_shared<raytracer::Sphere>(Vector3d::null(), 1);
}

static bool dummy = ElementFactory::self().registerClass<Sphere>("Sphere");

#include "Sphere.moc"
