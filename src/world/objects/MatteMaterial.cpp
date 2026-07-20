#include "world/objects/ElementFactory.h"
#include "world/objects/MatteMaterial.h"
#include "world/objects/Texture.h"

#include "render/materials/MatteMaterial.h"

MatteMaterial::MatteMaterial(Element* parent)
    : Material(parent),
      m_diffuseTexture(nullptr),
      m_normalTexture(nullptr),
      m_ambientCoefficient(1),
      m_diffuseCoefficient(1) {
}

void MatteMaterial::applyMatteMaterialProperties(
  const std::shared_ptr<render::MatteMaterial>& material) const {
  if (normalTexture()) {
    material->setNormalTexture(normalTexture()->toRaytracerTexture());
  }
  applyMaterialProperties(material);
}

std::shared_ptr<render::Material> MatteMaterial::toRaytracerMaterial() const {
  auto material =
    make_named<render::MatteMaterial>(textureOrDefault(diffuseTexture())->toRaytracerTexture());
  material->setAmbientCoefficient(ambientCoefficient());
  material->setDiffuseCoefficient(diffuseCoefficient());
  applyMatteMaterialProperties(material);

  return material;
}

static bool dummy = ElementFactory::self().registerClass<MatteMaterial>("MatteMaterial");
