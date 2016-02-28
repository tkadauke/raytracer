#include "world/objects/ElementFactory.h"
#include "world/objects/CheckerBoardTexture.h"
#include "raytracer/textures/CheckerBoardTexture.h"
#include "raytracer/textures/mappings/PlanarMapping2D.h"

CheckerBoardTexture::CheckerBoardTexture(Element* parent)
  : Texture(parent),
    m_brightTexture(nullptr),
    m_darkTexture(nullptr)
{
}

std::shared_ptr<raytracer::Texturec> CheckerBoardTexture::toRaytracerTexture() const {
  return std::make_shared<raytracer::CheckerBoardTexture>(
    new raytracer::PlanarMapping2D,
    textureOrDefault(brightTexture())->toRaytracerTexture(),
    textureOrDefault(darkTexture())->toRaytracerTexture()
  );
}

static bool dummy = ElementFactory::self().registerClass<CheckerBoardTexture>("CheckerBoardTexture");

#include "CheckerBoardTexture.moc"
