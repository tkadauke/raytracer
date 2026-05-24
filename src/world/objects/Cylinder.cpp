#include "world/objects/ElementFactory.h"
#include "world/objects/Cylinder.h"
#include "render/primitives/ClosedSolidUnion.h"
#include "render/primitives/OpenCylinder.h"
#include "render/primitives/Disk.h"
#include "render/primitives/Torus.h"
#include "render/primitives/Instance.h"
#include "render/materials/MatteMaterial.h"

Cylinder::Cylinder(Element* parent)
    : Surface(parent),
      m_radius(1),
      m_height(2),
      m_bevelRadius(0) {
}

std::shared_ptr<render::Primitive> Cylinder::toRaytracerPrimitive() const {
  auto result = make_named<render::ClosedSolidUnion>();

  result->add(make_named<render::OpenCylinder>(m_radius, m_height - 2.0 * m_bevelRadius));
  result->add(make_named<render::Disk>(Vector3d(0, -m_height / 2.0, 0), -Vector3d::up(),
                                       m_radius - m_bevelRadius));
  result->add(make_named<render::Disk>(Vector3d(0, m_height / 2.0, 0), Vector3d::up(),
                                       m_radius - m_bevelRadius));

  if (m_bevelRadius > 0.0) {
    for (int sign : {-1, 1}) {
      auto instance = make_named<render::Instance>(
        make_named<render::Torus>(m_radius - m_bevelRadius, m_bevelRadius));
      instance->setMatrix(Matrix4d::translate(0, sign * ((m_height / 2.0) - m_bevelRadius), 0));
      result->add(instance);
    }
  }

  return result;
}

static bool dummy = ElementFactory::self().registerClass<Cylinder>("Cylinder");
