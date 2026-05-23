#include "world/objects/ImageTexture.h"

#include "render/textures/ImageTexture.h"
#include "render/textures/mappings/PlanarMapping2D.h"
#include "render/textures/mappings/UVMapping2D.h"
#include "world/objects/ElementFactory.h"

ImageTexture::ImageTexture(Element* parent)
    : Texture(parent),
      m_filter("nearest"),
      m_wrap("repeat"),
      m_mapping("uv"),
      m_uScale(1.0),
      m_vScale(1.0) {
}

std::shared_ptr<render::Texturec> ImageTexture::toRaytracerTexture() const {
  render::TextureMapping2D* textureMapping =
    m_mapping == "planar"
      ? static_cast<render::TextureMapping2D*>(new render::PlanarMapping2D)
      : static_cast<render::TextureMapping2D*>(new render::UVMapping2D(m_uScale, m_vScale));

  return render::ImageTexture::fromFile(
    textureMapping, m_path.toStdString(),
    render::ImageTexture::filterFromString(m_filter.toStdString()),
    render::ImageTexture::wrapFromString(m_wrap.toStdString()));
}

static bool dummy = ElementFactory::self().registerClass<ImageTexture>("ImageTexture");
