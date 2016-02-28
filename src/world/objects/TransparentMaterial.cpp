#include "world/objects/ElementFactory.h"
#include "world/objects/TransparentMaterial.h"
#include "world/objects/Texture.h"

#include "raytracer/materials/TransparentMaterial.h"

TransparentMaterial::TransparentMaterial(Element* parent)
  : PhongMaterial(parent),
    m_refractionIndex(1)
{
}

raytracer::Material* TransparentMaterial::toRaytracerMaterial() const {
  auto material = new raytracer::TransparentMaterial;
  material->setAmbientCoefficient(ambientCoefficient());
  material->setDiffuseCoefficient(diffuseCoefficient());
  material->setSpecularCoefficient(specularCoefficient());
  material->setDiffuseTexture(textureOrDefault(diffuseTexture())->toRaytracerTexture());
  material->setSpecularColor(specularColor());
  material->setExponent(exponent());
  material->setRefractionIndex(refractionIndex());
  
  return material;
}

static bool dummy = ElementFactory::self().registerClass<TransparentMaterial>("TransparentMaterial");

#include "TransparentMaterial.moc"
