#include "world/objects/ElementFactory.h"
#include "world/objects/Rectangle.h"
#include "render/primitives/Rectangle.h"

Rectangle::Rectangle(Element* parent)
    : Surface(parent),
      m_leg1(1, 0, 0),
      m_leg2(0, 0, 1) {
}

std::shared_ptr<render::Primitive> Rectangle::toRaytracerPrimitive() const {
  // Local frame: corner at origin, two legs as configured. The
  // Surface base wraps this in an Instance carrying the position /
  // rotation transform.
  return make_named<render::Rectangle>(Vector3d::null, m_leg1, m_leg2);
}

bool Rectangle::supportsPlanarSceneMarker() const {
  return true;
}

static bool dummy = ElementFactory::self().registerClass<Rectangle>("Rectangle");
