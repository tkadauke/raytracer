#include "world/objects/ElementFactory.h"
#include "world/objects/ReflectiveMaterial.h"

#include "raytracer/materials/ReflectiveMaterial.h"

ReflectiveMaterial::ReflectiveMaterial(Element* parent)
  : PhongMaterial(parent), m_reflectionColor(Colord::white()), m_reflectionCoefficient(0.5)
{
}

raytracer::Material* ReflectiveMaterial::toRaytracerMaterial() const {
  auto material = new raytracer::ReflectiveMaterial(diffuseColor(), specularColor());
  material->setAmbientCoefficient(ambientCoefficient());
  material->setDiffuseCoefficient(diffuseCoefficient());
  material->setExponent(exponent());
  material->setSpecularCoefficient(specularCoefficient());
  material->setReflectionColor(reflectionColor());
  material->setReflectionCoefficient(reflectionCoefficient());
  
  return material;
}

static bool dummy = ElementFactory::self().registerClass<ReflectiveMaterial>("ReflectiveMaterial");

#include "ReflectiveMaterial.moc"
