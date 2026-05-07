#include "world/objects/ElementFactory.h"
#include "world/objects/Torus.h"
#include "render/primitives/Torus.h"

Torus::Torus(Element* parent)
  : Surface(parent),
    m_sweptRadius(2),
    m_tubeRadius(1)
{
}

std::shared_ptr<render::Primitive> Torus::toRaytracerPrimitive() const {
  return make_named<render::Torus>(m_sweptRadius, m_tubeRadius);
}

static bool dummy = ElementFactory::self().registerClass<Torus>("Torus");
