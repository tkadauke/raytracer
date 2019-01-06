#include "world/objects/ElementFactory.h"
#include "world/objects/Cylinder.h"
#include "raytracer/primitives/ClosedSolidUnion.h"
#include "raytracer/primitives/OpenCylinder.h"
#include "raytracer/primitives/Disk.h"
#include "raytracer/primitives/Torus.h"
#include "raytracer/primitives/Instance.h"
#include "raytracer/materials/MatteMaterial.h"

Cylinder::Cylinder(Element* parent)
  : Surface(parent),
    m_radius(1),
    m_height(2),
    m_bevelRadius(0)
{
}

std::shared_ptr<raytracer::Primitive> Cylinder::toRaytracerPrimitive() const {
  auto result = make_named<raytracer::ClosedSolidUnion>();

  result->add(make_named<raytracer::OpenCylinder>(m_radius, m_height - 2.0 * m_bevelRadius));
  result->add(make_named<raytracer::Disk>(
    Vector3d(0, -m_height/2.0, 0), -Vector3d::up(), m_radius - m_bevelRadius
  ));
  result->add(make_named<raytracer::Disk>(
    Vector3d(0,  m_height/2.0, 0),  Vector3d::up(), m_radius - m_bevelRadius
  ));

  if (m_bevelRadius > 0.0) {
    for (int sign : { -1, 1 }) {
      auto instance = make_named<raytracer::Instance>(
        make_named<raytracer::Torus>(m_radius - m_bevelRadius, m_bevelRadius)
      );
      instance->setMatrix(Matrix4d::translate(0, sign * ((m_height / 2.0) - m_bevelRadius), 0));
      result->add(instance);
    }
  }

  return result;
}

static bool dummy = ElementFactory::self().registerClass<Cylinder>("Cylinder");

#include "Cylinder.moc"
