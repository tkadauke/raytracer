#include "world/objects/ElementFactory.h"
#include "world/objects/ConstantColorTexture.h"
#include "render/textures/ConstantColorTexture.h"

ConstantColorTexture::ConstantColorTexture(Element* parent)
    : Texture(parent),
      m_color(Colord::black()) {
}

std::shared_ptr<render::Texturec> ConstantColorTexture::toRaytracerTexture() const {
  return make_named<render::ConstantColorTexture>(m_color);
}

static bool dummy =
  ElementFactory::self().registerClass<ConstantColorTexture>("ConstantColorTexture");
