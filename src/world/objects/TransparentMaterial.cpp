#include "world/objects/ElementFactory.h"
#include "world/objects/TransparentMaterial.h"

#include "raytracer/materials/TransparentMaterial.h"

TransparentMaterial::TransparentMaterial(Element* parent)
  : PhongMaterial(parent), m_refractionIndex(1)
{
}

raytracer::Material* TransparentMaterial::toRaytracerMaterial() const {
  auto material = new raytracer::TransparentMaterial;
  material->setAmbientCoefficient(ambientCoefficient());
  material->setDiffuseCoefficient(diffuseCoefficient());
  material->setSpecularCoefficient(specularCoefficient());
  material->setDiffuseColor(diffuseColor());
  material->setSpecularColor(specularColor());
  material->setExponent(exponent());
  material->setRefractionIndex(refractionIndex());
  
  return material;
}

static bool dummy = ElementFactory::self().registerClass<TransparentMaterial>("TransparentMaterial");

#include "TransparentMaterial.moc"
