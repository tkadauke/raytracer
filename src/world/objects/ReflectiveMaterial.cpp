#include "world/objects/ElementFactory.h"
#include "world/objects/ReflectiveMaterial.h"
#include "world/objects/Texture.h"

#include "render/materials/ReflectiveMaterial.h"

ReflectiveMaterial::ReflectiveMaterial(Element* parent)
    : PhongMaterial(parent),
      m_reflectionColor(Colord::white()),
      m_reflectionCoefficient(0.5) {
}

std::shared_ptr<render::Material> ReflectiveMaterial::toRaytracerMaterial() const {
  auto material = make_named<render::ReflectiveMaterial>(
    textureOrDefault(diffuseTexture())->toRaytracerTexture(), specularColor());
  material->setAmbientCoefficient(ambientCoefficient());
  material->setDiffuseCoefficient(diffuseCoefficient());
  material->setExponent(exponent());
  material->setSpecularCoefficient(specularCoefficient());
  material->setReflectionColor(reflectionColor());
  material->setReflectionCoefficient(reflectionCoefficient());
  applyMaterialProperties(material);

  return material;
}

static bool dummy = ElementFactory::self().registerClass<ReflectiveMaterial>("ReflectiveMaterial");
