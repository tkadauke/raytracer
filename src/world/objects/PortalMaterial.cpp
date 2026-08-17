#include "world/objects/ElementFactory.h"
#include "world/objects/PortalMaterial.h"
#include "world/objects/TransformComposition.h"

#include "render/materials/PortalMaterial.h"

PortalMaterial::PortalMaterial(Element* parent)
    : Material(parent),
      m_position(Vector3d::null),
      m_rotation(Vector3d::null),
      m_scale(Vector3d::one),
      m_filterColor(Colord::white()) {
}

std::shared_ptr<render::Material> PortalMaterial::toRaytracerMaterial() const {
  auto material = make_named<render::PortalMaterial>(portalTransform(), filterColor());
  applyMaterialProperties(material);
  return material;
}

Matrix4d PortalMaterial::portalTransform() const {
  return composePositionRotationScale(position(), rotation(), scale());
}

static bool dummy = ElementFactory::self().registerClass<PortalMaterial>("PortalMaterial");
