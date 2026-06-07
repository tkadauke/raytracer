#include "render/lights/PointLight.h"

using namespace render;

Vector3d PointLight::direction(const Vector3d& point) const {
  return (position() - point).normalized();
}

Colord PointLight::radiance() const {
  return color();
}

LightSample PointLight::sample(const Vector3d& point) const {
  return sample(point, Vector2d(0.5, 0.5));
}

LightSample PointLight::sample(const Vector3d& point, const Vector2d&) const {
  const Vector3d offset = position() - point;
  return {offset.normalized(), radiance(), offset.length(), 1.0, true};
}

std::optional<Colord> PointLight::power() const {
  return color();
}

const char* PointLight::fingerprintType() const {
  return "PointLight";
}

void PointLight::writeFingerprint(std::ostream& out, const std::string& prefix) const {
  writeCommonFingerprint(out, prefix);
  writeFingerprintVector(out, prefix + "position", position());
}

std::optional<Vector3d> PointLight::positionalLightPosition() const {
  return position();
}
