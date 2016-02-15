#include "world/objects/ElementFactory.h"
#include "world/objects/MatteMaterial.h"

#include "raytracer/materials/MatteMaterial.h"

MatteMaterial::MatteMaterial(Element* parent)
  : Material(parent), m_ambientCoefficient(1), m_diffuseCoefficient(1)
{
}

raytracer::Material* MatteMaterial::toRaytracerMaterial() const {
  auto material = new raytracer::MatteMaterial(diffuseColor());
  material->setAmbientCoefficient(ambientCoefficient());
  material->setDiffuseCoefficient(diffuseCoefficient());
  
  return material;
}

static bool dummy = ElementFactory::self().registerClass<MatteMaterial>("MatteMaterial");

#include "MatteMaterial.moc"
