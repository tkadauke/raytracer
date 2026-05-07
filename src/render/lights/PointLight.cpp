#include "render/lights/PointLight.h"

using namespace render;

Vector3d PointLight::direction(const Vector3d& point) const {
  return (position() - point).normalized();
}

Colord PointLight::radiance() const {
  return color();
}
