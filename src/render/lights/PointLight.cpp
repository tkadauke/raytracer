#include "render/lights/PointLight.h"

using namespace render;

Vector3d PointLight::direction(const Vector3d& point) const {
  return (position() - point).normalized();
}

Colord PointLight::radiance() const {
  return color();
}

const char* PointLight::fingerprintType() const {
  return "PointLight";
}

void PointLight::writeFingerprint(std::ostream& out, const std::string& prefix) const {
  writeCommonFingerprint(out, prefix);
  writeFingerprintVector(out, prefix + "position", position());
}
