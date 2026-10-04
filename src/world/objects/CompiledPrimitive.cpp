#include "world/objects/CompiledPrimitive.h"

#include "render/primitives/Primitive.h"
#include "world/objects/Element.h"

CompiledPrimitive::CompiledPrimitive(std::shared_ptr<render::Primitive> primitive,
                                     Element* parent)
    : Surface(parent),
      m_primitive(std::move(primitive)) {
  setGenerated(true);
}

std::shared_ptr<render::Primitive> CompiledPrimitive::toRaytracerPrimitive() const {
  return m_primitive;
}

CompiledPrimitive* CompiledPrimitive::attachTo(Element& parent,
                                               std::shared_ptr<render::Primitive> primitive,
                                               const QString& fallbackName) {
  auto compiled = std::make_unique<CompiledPrimitive>(std::move(primitive));
  compiled->setId(parent.id() + ":compiled-geometry");
  compiled->setName(parent.name().isEmpty() ? fallbackName : parent.name() + " Geometry");
  CompiledPrimitive* raw = compiled.get();
  parent.addChild(std::move(compiled));
  return raw;
}
