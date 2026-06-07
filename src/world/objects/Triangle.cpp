#include "world/objects/ElementFactory.h"
#include "world/objects/Triangle.h"
#include "render/primitives/Triangle.h"

Triangle::Triangle(Element* parent)
    : Surface(parent),
      m_vertexA(1, 0, 0),
      m_vertexB(-1, 0, 0),
      m_vertexC(0, -1, 0) {
}

std::shared_ptr<render::Primitive> Triangle::toRaytracerPrimitive() const {
  return make_named<render::Triangle>(m_vertexA, m_vertexB, m_vertexC);
}

bool Triangle::supportsPlanarSceneMarker() const {
  return true;
}

static bool dummy = ElementFactory::self().registerClass<Triangle>("Triangle");
