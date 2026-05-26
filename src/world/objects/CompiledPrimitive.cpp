#include "world/objects/CompiledPrimitive.h"

#include "render/primitives/Primitive.h"

CompiledPrimitive::CompiledPrimitive(std::shared_ptr<render::Primitive> primitive,
                                     Element* parent)
    : Surface(parent),
      m_primitive(std::move(primitive)) {
  setGenerated(true);
}

std::shared_ptr<render::Primitive> CompiledPrimitive::toRaytracerPrimitive() const {
  return m_primitive;
}
