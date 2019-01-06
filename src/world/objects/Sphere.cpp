#include "world/objects/ElementFactory.h"
#include "world/objects/Sphere.h"
#include "raytracer/primitives/Sphere.h"
#include "raytracer/materials/MatteMaterial.h"

Sphere::Sphere(Element* parent)
  : Surface(parent),
    m_radius(1)
{
}

std::shared_ptr<raytracer::Primitive> Sphere::toRaytracerPrimitive() const {
  return make_named<raytracer::Sphere>(Vector3d::null(), m_radius);
}

static bool dummy = ElementFactory::self().registerClass<Sphere>("Sphere");

#include "Sphere.moc"
