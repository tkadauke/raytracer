#include "world/objects/RectangularAreaLight.h"

#include "render/lights/RectangularAreaLight.h"
#include "world/objects/ElementFactory.h"

#include <algorithm>
#include <cmath>
#include <limits>

RectangularAreaLight::RectangularAreaLight(Element* parent)
    : Light(parent),
      m_width(2.0),
      m_height(2.0) {
}

double RectangularAreaLight::width() const {
  return m_width;
}

void RectangularAreaLight::setWidth(double width) {
  m_width = std::max(std::abs(width), std::numeric_limits<double>::epsilon());
}

double RectangularAreaLight::height() const {
  return m_height;
}

void RectangularAreaLight::setHeight(double height) {
  m_height = std::max(std::abs(height), std::numeric_limits<double>::epsilon());
}

std::shared_ptr<render::Light> RectangularAreaLight::toRaytracer() const {
  const Matrix4d transform = globalTransform();
  const Matrix3d linearTransform(transform);
  return make_named<render::RectangularAreaLight>(
    transform * Vector3d::null, linearTransform * Vector3d(width(), 0.0, 0.0),
    linearTransform * Vector3d(0.0, 0.0, height()), color() * intensity());
}

static bool dummy =
  ElementFactory::self().registerClass<RectangularAreaLight>("RectangularAreaLight");
