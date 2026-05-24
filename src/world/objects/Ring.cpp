#include "world/objects/ElementFactory.h"
#include "world/objects/Ring.h"
#include "render/primitives/ClosedSolidUnion.h"
#include "render/primitives/Union.h"
#include "render/primitives/Difference.h"
#include "render/primitives/OpenCylinder.h"
#include "render/primitives/Disk.h"
#include "render/primitives/Torus.h"
#include "render/primitives/Instance.h"
#include "render/materials/MatteMaterial.h"

Ring::Ring(Element* parent)
    : Surface(parent),
      m_innerRadius(0.5),
      m_outerRadius(1),
      m_height(2),
      m_bevelRadius(0) {
}

std::shared_ptr<render::Primitive> Ring::toRaytracerPrimitive() const {
  double br = bevelRadius();

  if (br == 0.0) {
    return ring(m_outerRadius, m_innerRadius, m_height);
  } else if (isAlmost(br, (m_outerRadius - m_innerRadius) / 2.0)) {
    auto result = make_named<render::Union>();
    result->add(ring(m_outerRadius, m_innerRadius, m_height - 2.0 * br));

    for (int sign : {-1, 1}) {
      auto instance =
        make_named<render::Instance>(make_named<render::Torus>(m_outerRadius - br, br));
      instance->setMatrix(Matrix4d::translate(0, sign * ((m_height / 2.0) - br), 0));
      result->add(instance);
    }
    return result;
  } else {
    auto result = make_named<render::Union>();
    result->add(ring(m_outerRadius, m_innerRadius, m_height - 2.0 * br));
    result->add(ring(m_outerRadius - br, m_innerRadius + br, m_height));

    for (int sign : {-1, 1}) {
      for (double radius : {m_outerRadius - br, m_innerRadius + br}) {
        auto instance = make_named<render::Instance>(make_named<render::Torus>(radius, br));
        instance->setMatrix(Matrix4d::translate(0, sign * ((m_height / 2.0) - br), 0));
        result->add(instance);
      }
    }

    return result;
  }
}

std::shared_ptr<render::Primitive> Ring::closedCylinder(double radius, double height) const {
  auto result = make_named<render::ClosedSolidUnion>();

  result->add(make_named<render::OpenCylinder>(radius, height));
  for (int sign : {-1, 1}) {
    result->add(
      make_named<render::Disk>(Vector3d(0, sign * height / 2.0, 0), Vector3d::up() * sign, radius));
  }
  return result;
}

std::shared_ptr<render::Primitive> Ring::ring(double outerRadius, double innerRadius,
                                              double height) const {
  if (isAlmostZero(innerRadius)) {
    return closedCylinder(outerRadius, height);
  } else {
    auto result = make_named<render::Difference>();

    result->add(closedCylinder(outerRadius, height));
    result->add(closedCylinder(innerRadius, height + 0.0001));

    return result;
  }
}

static bool dummy = ElementFactory::self().registerClass<Ring>("Ring");
