#include "world/objects/ElementFactory.h"
#include "world/objects/MatteMaterial.h"
#include "world/objects/Texture.h"

#include "raytracer/materials/MatteMaterial.h"

MatteMaterial::MatteMaterial(Element* parent)
  : Material(parent), m_diffuseTexture(nullptr), m_ambientCoefficient(1), m_diffuseCoefficient(1)
{
}

raytracer::Material* MatteMaterial::toRaytracerMaterial() const {
  auto texture = diffuseTexture() ? diffuseTexture() : Texture::defaultTexture();
  auto material = new raytracer::MatteMaterial(texture->toRaytracerTexture());
  material->setAmbientCoefficient(ambientCoefficient());
  material->setDiffuseCoefficient(diffuseCoefficient());
  
  return material;
}

static bool dummy = ElementFactory::self().registerClass<MatteMaterial>("MatteMaterial");

#include "MatteMaterial.moc"
