#include "world/objects/ElementFactory.h"
#include "world/objects/OpenCylinder.h"
#include "render/primitives/OpenCylinder.h"

OpenCylinder::OpenCylinder(Element* parent)
  : Surface(parent),
    m_radius(1),
    m_height(2)
{
}

std::shared_ptr<render::Primitive> OpenCylinder::toRaytracerPrimitive() const {
  return make_named<render::OpenCylinder>(m_radius, m_height);
}

static bool dummy = ElementFactory::self().registerClass<OpenCylinder>("OpenCylinder");
