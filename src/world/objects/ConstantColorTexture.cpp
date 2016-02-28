#include "world/objects/ElementFactory.h"
#include "world/objects/ConstantColorTexture.h"
#include "raytracer/textures/ConstantColorTexture.h"

ConstantColorTexture::ConstantColorTexture(Element* parent)
  : Texture(parent),
    m_color(Colord::black())
{
}

std::shared_ptr<raytracer::Texturec> ConstantColorTexture::toRaytracerTexture() const {
  return std::make_shared<raytracer::ConstantColorTexture>(m_color);
}

static bool dummy = ElementFactory::self().registerClass<ConstantColorTexture>("ConstantColorTexture");

#include "ConstantColorTexture.moc"
