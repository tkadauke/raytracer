#include "world/objects/ElementFactory.h"
#include "world/objects/UVColorTexture.h"
#include "render/textures/UVColorTexture.h"

UVColorTexture::UVColorTexture(Element* parent)
  : Texture(parent)
{
}

std::shared_ptr<render::Texturec> UVColorTexture::toRaytracerTexture() const {
  return make_named<render::UVColorTexture>();
}

static bool dummy = ElementFactory::self().registerClass<UVColorTexture>("UVColorTexture");
