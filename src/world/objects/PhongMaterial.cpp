#include "world/objects/ElementFactory.h"
#include "world/objects/PhongMaterial.h"
#include "world/objects/Texture.h"

#include "render/materials/PhongMaterial.h"

PhongMaterial::PhongMaterial(Element* parent)
    : MatteMaterial(parent),
      m_specularColor(Colord::white()),
      m_exponent(16),
      m_specularCoefficient(1) {
}

void PhongMaterial::applyPhongMaterialProperties(
  const std::shared_ptr<render::PhongMaterial>& material) const {
  material->setExponent(exponent());
  material->setSpecularCoefficient(specularCoefficient());
  applyMatteMaterialProperties(material);
}

std::shared_ptr<render::Material> PhongMaterial::toRaytracerMaterial() const {
  auto material = make_named<render::PhongMaterial>(
    textureOrDefault(diffuseTexture())->toRaytracerTexture(), specularColor(), exponent());
  applyPhongMaterialProperties(material);

  return material;
}

static bool dummy = ElementFactory::self().registerClass<PhongMaterial>("PhongMaterial");
