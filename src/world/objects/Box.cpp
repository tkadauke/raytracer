#include "world/objects/ElementFactory.h"
#include "world/objects/Box.h"
#include "raytracer/primitives/Box.h"
#include "raytracer/materials/MatteMaterial.h"

Box::Box(Element* parent)
  : Surface(parent),
    m_size(Vector3d::one())
{
}

std::shared_ptr<raytracer::Primitive> Box::toRaytracerPrimitive() const {
  return std::make_shared<raytracer::Box>(Vector3d::null(), m_size);
}

static bool dummy = ElementFactory::self().registerClass<Box>("Box");

#include "Box.moc"
