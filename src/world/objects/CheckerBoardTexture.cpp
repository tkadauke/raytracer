#include "world/objects/ElementFactory.h"
#include "world/objects/CheckerBoardTexture.h"
#include "render/textures/CheckerBoardTexture.h"
#include "render/textures/mappings/PlanarMapping2D.h"
#include "render/textures/mappings/UVMapping2D.h"

#include <QVariant>

namespace {
  QString effectiveMapping(const CheckerBoardTexture& texture) {
    const QVariant value = texture.property("mapping");
    const QString mapping = value.isValid() ? value.toString() : texture.mapping();
    return mapping == "uv" ? "uv" : "planar";
  }

  double effectiveScale(const CheckerBoardTexture& texture, const char* property, double fallback) {
    const QVariant value = texture.property(property);
    return value.isValid() ? value.toDouble() : fallback;
  }
}

CheckerBoardTexture::CheckerBoardTexture(Element* parent)
  : Texture(parent),
    m_brightTexture(nullptr),
    m_darkTexture(nullptr),
    m_mapping("planar"),
    m_uScale(1.0),
    m_vScale(1.0)
{
}

std::shared_ptr<render::Texturec> CheckerBoardTexture::toRaytracerTexture() const {
  const QString mappingName = effectiveMapping(*this);
  const double uScale = effectiveScale(*this, "uScale", m_uScale);
  const double vScale = effectiveScale(*this, "vScale", m_vScale);
  render::TextureMapping2D* mapping =
    mappingName == "uv"
      ? static_cast<render::TextureMapping2D*>(new render::UVMapping2D(uScale, vScale))
      : static_cast<render::TextureMapping2D*>(new render::PlanarMapping2D);
  return make_named<render::CheckerBoardTexture>(
    mapping,
    textureOrDefault(brightTexture())->toRaytracerTexture(),
    textureOrDefault(darkTexture())->toRaytracerTexture()
  );
}

static bool dummy = ElementFactory::self().registerClass<CheckerBoardTexture>("CheckerBoardTexture");
