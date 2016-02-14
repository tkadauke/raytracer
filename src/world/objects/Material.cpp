#include "world/objects/Material.h"
#include "world/objects/MatteMaterial.h"

Material* Material::defaultMaterial() {
  static MatteMaterial* result = 0;
  if (!result) {
    result = new MatteMaterial(nullptr);
    result->setDiffuseColor(Colord(1, 1, 1));
  }
  
  return result;
}


Material::Material(Element* parent)
  : Element(parent)
{
}

#include "Material.moc"
