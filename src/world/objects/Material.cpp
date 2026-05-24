#include "world/objects/Material.h"
#include "world/objects/MatteMaterial.h"

#include "render/materials/Material.h"

Material* Material::defaultMaterial() {
  static MatteMaterial result(nullptr);
  return &result;
}

Material::Material(Element* parent)
    : Element(parent),
      m_sidedness(Sidedness::TwoSided) {
}

void Material::setSidedness(Sidedness sidedness) {
  m_sidedness = sidedness;
}

QString Material::sidednessName() const {
  switch (m_sidedness) {
  case Sidedness::Front:
    return "Front";
  case Sidedness::Back:
    return "Back";
  case Sidedness::TwoSided:
    return "TwoSided";
  }
  return "TwoSided";
}

void Material::setSidednessName(const QString& sidedness) {
  if (sidedness == "Front") {
    setSidedness(Sidedness::Front);
  } else if (sidedness == "Back") {
    setSidedness(Sidedness::Back);
  } else {
    setSidedness(Sidedness::TwoSided);
  }
}

void Material::applyMaterialProperties(const std::shared_ptr<render::Material>& material) const {
  if (!material)
    return;

  switch (m_sidedness) {
  case Sidedness::Front:
    material->setSidedness(render::Material::Sidedness::Front);
    break;
  case Sidedness::Back:
    material->setSidedness(render::Material::Sidedness::Back);
    break;
  case Sidedness::TwoSided:
    material->setSidedness(render::Material::Sidedness::TwoSided);
    break;
  }
}
