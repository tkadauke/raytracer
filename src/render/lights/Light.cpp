#include "render/lights/Light.h"

#include <ostream>

using namespace render;

void Light::writeFingerprint(std::ostream& out, const std::string& prefix) const {
  writeCommonFingerprint(out, prefix);
  writeFingerprintVector(out, prefix + "directionAtOrigin", direction(Vector3d::null));
}

std::optional<Vector3d> Light::directionalShadowMapDirection() const {
  return std::nullopt;
}

std::optional<Vector3d> Light::positionalLightPosition() const {
  return std::nullopt;
}

void Light::writeCommonFingerprint(std::ostream& out, const std::string& prefix) const {
  out << prefix << "type=" << fingerprintType() << ';';
  writeFingerprintColor(out, prefix + "radiance", radiance());
}

void Light::writeFingerprintColor(std::ostream& out, const std::string& name, const Colord& color) {
  out << name << '=' << color.r() << ',' << color.g() << ',' << color.b() << ';';
}

void Light::writeFingerprintVector(std::ostream& out, const std::string& name,
                                   const Vector3d& vector) {
  out << name << '=' << vector.x() << ',' << vector.y() << ',' << vector.z() << ';';
}
