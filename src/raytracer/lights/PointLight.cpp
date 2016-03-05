#include "raytracer/lights/PointLight.h"

using namespace raytracer;

Vector3d PointLight::direction(const Vector3d& point) const {
  return (position() - point).normalized();
}

Colord PointLight::radiance() const {
  return color();
}
