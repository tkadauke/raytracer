#include "world/objects/Material.h"
#include "world/objects/MatteMaterial.h"

Material* Material::defaultMaterial() {
  static MatteMaterial result(nullptr);
  return &result;
}


Material::Material(Element* parent)
  : Element(parent)
{
}

#include "Material.moc"
