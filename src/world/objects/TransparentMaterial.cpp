#include "world/objects/ElementFactory.h"
#include "world/objects/TransparentMaterial.h"
#include "world/objects/Texture.h"

#include "render/materials/TransparentMaterial.h"

TransparentMaterial::TransparentMaterial(Element* parent)
  : PhongMaterial(parent),
    m_transmissionCoefficient(1),
    m_refractionIndex(1)
{
}

std::shared_ptr<render::Material> TransparentMaterial::toRaytracerMaterial() const {
  auto material = make_named<render::TransparentMaterial>();
  material->setAmbientCoefficient(ambientCoefficient());
  material->setDiffuseCoefficient(diffuseCoefficient());
  material->setSpecularCoefficient(specularCoefficient());
  material->setDiffuseTexture(textureOrDefault(diffuseTexture())->toRaytracerTexture());
  material->setSpecularColor(specularColor());
  material->setExponent(exponent());
  material->setTransmissionCoefficient(transmissionCoefficient());
  material->setRefractionIndex(refractionIndex());
  material->setReflectionColor(reflectionColor());
  material->setReflectionCoefficient(reflectionCoefficient());

  return material;
}

static bool dummy = ElementFactory::self().registerClass<TransparentMaterial>("TransparentMaterial");

