#include "world/objects/ElementFactory.h"
#include "world/objects/CheckerBoardTexture.h"
#include "render/textures/CheckerBoardTexture.h"
#include "render/textures/mappings/PlanarMapping2D.h"

CheckerBoardTexture::CheckerBoardTexture(Element* parent)
  : Texture(parent),
    m_brightTexture(nullptr),
    m_darkTexture(nullptr)
{
}

std::shared_ptr<render::Texturec> CheckerBoardTexture::toRaytracerTexture() const {
  return make_named<render::CheckerBoardTexture>(
    new render::PlanarMapping2D,
    textureOrDefault(brightTexture())->toRaytracerTexture(),
    textureOrDefault(darkTexture())->toRaytracerTexture()
  );
}

static bool dummy = ElementFactory::self().registerClass<CheckerBoardTexture>("CheckerBoardTexture");

