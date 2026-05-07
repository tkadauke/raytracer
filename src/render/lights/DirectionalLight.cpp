#include "render/lights/DirectionalLight.h"

using namespace render;

Vector3d DirectionalLight::direction(const Vector3d&) const {
  return direction();
}

Colord DirectionalLight::radiance() const {
  return color();
}
