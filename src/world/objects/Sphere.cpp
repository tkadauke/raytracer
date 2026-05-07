#include "world/objects/ElementFactory.h"
#include "world/objects/Sphere.h"
#include "render/primitives/Sphere.h"
#include "render/materials/MatteMaterial.h"

Sphere::Sphere(Element* parent)
  : Surface(parent),
    m_radius(1)
{
}

std::shared_ptr<render::Primitive> Sphere::toRaytracerPrimitive() const {
  return make_named<render::Sphere>(Vector3d::null(), m_radius);
}

static bool dummy = ElementFactory::self().registerClass<Sphere>("Sphere");

