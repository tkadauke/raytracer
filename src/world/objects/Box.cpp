#include "world/objects/ElementFactory.h"
#include "world/objects/Box.h"
#include "raytracer/primitives/Box.h"
#include "raytracer/primitives/ClosedSolidUnion.h"
#include "raytracer/primitives/OpenCylinder.h"
#include "raytracer/primitives/Sphere.h"
#include "raytracer/primitives/Instance.h"
#include "raytracer/materials/MatteMaterial.h"

Box::Box(Element* parent)
  : Surface(parent),
    m_size(Vector3d::one()),
    m_bevelRadius(0)
{
}

std::shared_ptr<raytracer::Primitive> Box::toRaytracerPrimitive() const {
  const Vector3d& s = size();
  const double r = bevelRadius();

  if (r == 0.0) {
    return std::make_shared<raytracer::Box>(Vector3d::null(), s);
  } else if (r == s.min()) {
    return std::make_shared<raytracer::Sphere>(Vector3d::null(), r);
  } else {
    auto result = std::make_shared<raytracer::ClosedSolidUnion>();
    
    for (int i = 0; i != 8; i++) {
      Vector3d center(
        (s.x() - r) * (i & 0x1 ? -1 : 1),
        (s.y() - r) * (i & 0x2 ? -1 : 1),
        (s.z() - r) * (i & 0x4 ? -1 : 1)
      );
      
      result->add(std::make_shared<raytracer::Sphere>(center, r));
    }
    
    result->add(std::make_shared<raytracer::Box>(
      Vector3d::null(),
      Vector3d(s.x(), s.y() - r, s.z() - r)
    ));

    result->add(std::make_shared<raytracer::Box>(
      Vector3d::null(),
      Vector3d(s.x() - r, s.y(), s.z() - r)
    ));
    
    result->add(std::make_shared<raytracer::Box>(
      Vector3d::null(),
      Vector3d(s.x() - r, s.y() - r, s.z())
    ));
      
    for (int u = -1; u <= 1; u += 2) {
      for (int v = -1; v <= 1; v += 2) {
        auto cylinder = std::make_shared<raytracer::OpenCylinder>(r, 2.0 * (s.x() - r));
        auto instance = std::make_shared<raytracer::Instance>(cylinder);
        instance->setMatrix(
          Matrix4d::translate(0, u * (s.y() - r), v * (s.z() - r))
        * Matrix3d::rotateZ(Angled::fromDegrees(90))
        );
        result->add(instance);
        
        cylinder = std::make_shared<raytracer::OpenCylinder>(r, 2.0 * (s.y() - r));
        instance = std::make_shared<raytracer::Instance>(cylinder);
        instance->setMatrix(
          Matrix4d::translate(u * (s.x() - r), 0, v * (s.z() - r))
        );
        result->add(instance);
        
        cylinder = std::make_shared<raytracer::OpenCylinder>(r, 2.0 * (s.z() - r));
        instance = std::make_shared<raytracer::Instance>(cylinder);
        instance->setMatrix(
          Matrix4d::translate(u * (s.x() - r), v * (s.y() - r), 0)
        * Matrix3d::rotateX(Angled::fromDegrees(90))
        );
        result->add(instance);
      }
    }
    
    return result;
  }
}

static bool dummy = ElementFactory::self().registerClass<Box>("Box");

#include "Box.moc"
