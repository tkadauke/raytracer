#include "world/objects/ElementFactory.h"
#include "world/objects/Box.h"
#include "render/primitives/Box.h"
#include "render/primitives/ClosedSolidUnion.h"
#include "render/primitives/OpenCylinder.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Instance.h"
#include "render/materials/MatteMaterial.h"

Box::Box(Element* parent)
  : Surface(parent),
    m_size(Vector3d::one),
    m_bevelRadius(0)
{
}

std::shared_ptr<render::Primitive> Box::toRaytracerPrimitive() const {
  const Vector3d& s = size();
  const double r = bevelRadius();

  if (r == 0.0) {
    return make_named<render::Box>(Vector3d::null, s);
  } else if (r == s.min()) {
    return make_named<render::Sphere>(Vector3d::null, r);
  } else {
    auto result = make_named<render::ClosedSolidUnion>();

    for (int i = 0; i != 8; i++) {
      Vector3d center(
        (s.x() - r) * ((i & 0x1) ? -1 : 1),
        (s.y() - r) * ((i & 0x2) ? -1 : 1),
        (s.z() - r) * ((i & 0x4) ? -1 : 1)
      );

      result->add(make_named<render::Sphere>(center, r));
    }

    result->add(make_named<render::Box>(
      Vector3d::null,
      Vector3d(s.x(), s.y() - r, s.z() - r)
    ));

    result->add(make_named<render::Box>(
      Vector3d::null,
      Vector3d(s.x() - r, s.y(), s.z() - r)
    ));

    result->add(make_named<render::Box>(
      Vector3d::null,
      Vector3d(s.x() - r, s.y() - r, s.z())
    ));

    for (int u : { -1, 1 }) {
      for (int v : { -1, 1 }) {
        auto cylinder = make_named<render::OpenCylinder>(r, 2.0 * (s.x() - r));
        auto instance = make_named<render::Instance>(cylinder);
        instance->setMatrix(
          Matrix4d::translate(0, u * (s.y() - r), v * (s.z() - r))
        * Matrix3d::rotateZ(90_degrees)
        );
        result->add(instance);

        cylinder = make_named<render::OpenCylinder>(r, 2.0 * (s.y() - r));
        instance = make_named<render::Instance>(cylinder);
        instance->setMatrix(
          Matrix4d::translate(u * (s.x() - r), 0, v * (s.z() - r))
        );
        result->add(instance);

        cylinder = make_named<render::OpenCylinder>(r, 2.0 * (s.z() - r));
        instance = make_named<render::Instance>(cylinder);
        instance->setMatrix(
          Matrix4d::translate(u * (s.x() - r), v * (s.y() - r), 0)
        * Matrix3d::rotateX(90_degrees)
        );
        result->add(instance);
      }
    }

    return result;
  }
}

static bool dummy = ElementFactory::self().registerClass<Box>("Box");

