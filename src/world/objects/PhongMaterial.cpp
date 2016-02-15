#include "world/objects/ElementFactory.h"
#include "world/objects/PhongMaterial.h"

#include "raytracer/materials/PhongMaterial.h"

PhongMaterial::PhongMaterial(Element* parent)
  : MatteMaterial(parent), m_highlightColor(Colord::white()), m_exponent(16)
{
}

raytracer::Material* PhongMaterial::toRaytracerMaterial() const {
  return new raytracer::PhongMaterial(diffuseColor(), highlightColor(), exponent());
}

static bool dummy = ElementFactory::self().registerClass<PhongMaterial>("PhongMaterial");

#include "PhongMaterial.moc"
