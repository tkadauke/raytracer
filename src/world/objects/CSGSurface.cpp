#include "world/objects/CSGSurface.h"

#include "render/primitives/Composite.h"

CSGSurface::CSGSurface(Element* parent)
    : Surface(parent),
      m_active(true) {
}

std::shared_ptr<render::Primitive> CSGSurface::csgInactivePrimitive() const {
  return make_named<render::Composite>();
}
