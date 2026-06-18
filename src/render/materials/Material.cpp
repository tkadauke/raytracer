#include "render/materials/Material.h"

#include "core/math/HitPoint.h"
#include "render/materials/EmissiveMaterial.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/PhongMaterial.h"
#include "render/materials/PortalMaterial.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/materials/TransparentMaterial.h"

namespace render {
  Rayd MaterialBsdfSample::rayFrom(const HitPoint& hitPoint) const {
    if (continuationRay) {
      return *continuationRay;
    }
    return Rayd(hitPoint.point(), direction).epsilonShifted();
  }

  void MaterialVisitor::visit(const Material&) {
  }

  void MaterialVisitor::visit(const MatteMaterial& material) {
    visit(static_cast<const Material&>(material));
  }

  void MaterialVisitor::visit(const PhongMaterial& material) {
    visit(static_cast<const MatteMaterial&>(material));
  }

  void MaterialVisitor::visit(const ReflectiveMaterial& material) {
    visit(static_cast<const PhongMaterial&>(material));
  }

  void MaterialVisitor::visit(const TransparentMaterial& material) {
    visit(static_cast<const PhongMaterial&>(material));
  }

  void MaterialVisitor::visit(const EmissiveMaterial& material) {
    visit(static_cast<const Material&>(material));
  }

  void MaterialVisitor::visit(const PortalMaterial& material) {
    visit(static_cast<const Material&>(material));
  }

  const char* Material::typeName() const noexcept {
    return "Material";
  }

  void Material::accept(MaterialVisitor& visitor) const {
    visitor.visit(*this);
  }
}
