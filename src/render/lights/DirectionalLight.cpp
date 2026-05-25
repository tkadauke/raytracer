#include "render/lights/DirectionalLight.h"

using namespace render;

Vector3d DirectionalLight::direction(const Vector3d&) const {
  return direction();
}

Colord DirectionalLight::radiance() const {
  return color();
}

const char* DirectionalLight::fingerprintType() const {
  return "DirectionalLight";
}

void DirectionalLight::writeFingerprint(std::ostream& out, const std::string& prefix) const {
  writeCommonFingerprint(out, prefix);
  writeFingerprintVector(out, prefix + "direction", direction());
}

std::optional<Vector3d> DirectionalLight::directionalShadowMapDirection() const {
  return direction();
}
