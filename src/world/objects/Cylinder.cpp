#include "world/objects/ElementFactory.h"
#include "world/objects/Cylinder.h"
#include "raytracer/primitives/ClosedSolidUnion.h"
#include "raytracer/primitives/OpenCylinder.h"
#include "raytracer/primitives/Disk.h"
#include "raytracer/materials/MatteMaterial.h"

Cylinder::Cylinder(Element* parent)
  : Surface(parent),
    m_radius(1),
    m_height(2)
{
}

std::shared_ptr<raytracer::Primitive> Cylinder::toRaytracerPrimitive() const {
  auto result = std::make_shared<raytracer::ClosedSolidUnion>();
  
  result->add(std::make_shared<raytracer::OpenCylinder>(m_radius, m_height));
  result->add(std::make_shared<raytracer::Disk>(
    Vector3d(0, -m_height/2.0, 0), -Vector3d::up(), m_radius
  ));
  result->add(std::make_shared<raytracer::Disk>(
    Vector3d(0,  m_height/2.0, 0),  Vector3d::up(), m_radius
  ));
  
  return result;
}

static bool dummy = ElementFactory::self().registerClass<Cylinder>("Cylinder");

#include "Cylinder.moc"
