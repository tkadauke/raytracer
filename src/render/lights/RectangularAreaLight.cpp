#include "render/lights/RectangularAreaLight.h"

#include "core/math/Constants.h"
#include "render/materials/EmissiveMaterial.h"
#include "render/primitives/Rectangle.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace render;

RectangularAreaLight::RectangularAreaLight(const Vector3d& center, const Vector3d& edgeU,
                                           const Vector3d& edgeV, const Colord& radiance)
    : m_center(center),
      m_edgeU(edgeU),
      m_edgeV(edgeV),
      m_normal((edgeU ^ edgeV).normalizedOrZero(tolerance)),
      m_radiance(radiance),
      m_area((edgeU ^ edgeV).length()) {
}

const Vector3d& RectangularAreaLight::center() const {
  return m_center;
}

const Vector3d& RectangularAreaLight::edgeU() const {
  return m_edgeU;
}

const Vector3d& RectangularAreaLight::edgeV() const {
  return m_edgeV;
}

const Vector3d& RectangularAreaLight::normal() const {
  return m_normal;
}

const Colord& RectangularAreaLight::color() const {
  return m_radiance;
}

double RectangularAreaLight::area() const {
  return m_area;
}

Vector3d RectangularAreaLight::direction(const Vector3d& point) const {
  return (center() - point).normalized();
}

Colord RectangularAreaLight::radiance() const {
  return m_radiance;
}

LightSample RectangularAreaLight::sample(const Vector3d& point) const {
  return sampleAtCanonicalPoint(point);
}

LightSample RectangularAreaLight::sample(const Vector3d& point, const Vector2d& sample) const {
  if (area() <= tolerance) {
    return {Vector3d::null, Colord::black(), std::numeric_limits<double>::infinity(), 0.0, false};
  }

  const Vector3d lightPoint = samplePoint(sample);
  const Vector3d offset = lightPoint - point;
  const double distance = offset.length();
  if (distance <= tolerance) {
    return {Vector3d::null, Colord::black(), 0.0, 0.0, false};
  }

  const Vector3d directionToLight = offset / distance;
  const double cosLight = surfaceCosine(directionToLight);
  if (cosLight <= tolerance) {
    return {directionToLight, Colord::black(), distance, 0.0, false};
  }

  const double solidAnglePdf = (distance * distance) / (cosLight * area());
  return {directionToLight, radiance(), distance, solidAnglePdf, false};
}

double RectangularAreaLight::pdf(const Vector3d& point, const Vector3d& direction) const {
  if (area() <= tolerance) {
    return 0.0;
  }

  const double normalDotDirection = normal() * direction;
  if (std::abs(normalDotDirection) <= tolerance) {
    return 0.0;
  }

  const double t = ((center() - point) * normal()) / normalDotDirection;
  if (t <= tolerance) {
    return 0.0;
  }

  const Vector3d lightPoint = point + direction * t;
  if (!containsPoint(lightPoint)) {
    return 0.0;
  }

  const double cosLight = surfaceCosine(direction.normalized());
  if (cosLight <= tolerance) {
    return 0.0;
  }

  return (t * t) / (cosLight * area());
}

bool RectangularAreaLight::isDelta() const {
  return false;
}

Colord RectangularAreaLight::emission() const {
  return radiance();
}

std::optional<Colord> RectangularAreaLight::power() const {
  return radiance() * area() * PI;
}

std::shared_ptr<render::Primitive> RectangularAreaLight::emitterPrimitive() const {
  if (area() <= tolerance) {
    return nullptr;
  }

  auto rectangle = std::make_shared<render::Rectangle>(center() - edgeU() * 0.5 - edgeV() * 0.5,
                                                       edgeU(), edgeV(), normal());
  rectangle->setMaterial(std::make_shared<render::EmissiveMaterial>(radiance()));
  return rectangle;
}

const char* RectangularAreaLight::fingerprintType() const {
  return "RectangularAreaLight";
}

void RectangularAreaLight::writeFingerprint(std::ostream& out, const std::string& prefix) const {
  writeCommonFingerprint(out, prefix);
  writeFingerprintVector(out, prefix + "center", center());
  writeFingerprintVector(out, prefix + "edgeU", edgeU());
  writeFingerprintVector(out, prefix + "edgeV", edgeV());
}

Vector3d RectangularAreaLight::samplePoint(const Vector2d& sample) const {
  return center() + edgeU() * (sample.x() - 0.5) + edgeV() * (sample.y() - 0.5);
}

bool RectangularAreaLight::containsPoint(const Vector3d& point) const {
  const Vector3d local = point - center();
  const double uu = edgeU() * edgeU();
  const double uv = edgeU() * edgeV();
  const double vv = edgeV() * edgeV();
  const double lu = local * edgeU();
  const double lv = local * edgeV();
  const double determinant = uu * vv - uv * uv;
  if (std::abs(determinant) <= tolerance) {
    return false;
  }

  const double u = (vv * lu - uv * lv) / determinant;
  const double v = (uu * lv - uv * lu) / determinant;
  return u >= -0.5 - tolerance && u <= 0.5 + tolerance && v >= -0.5 - tolerance &&
         v <= 0.5 + tolerance;
}

double RectangularAreaLight::surfaceCosine(const Vector3d& directionToLight) const {
  return std::max(0.0, normal() * -directionToLight);
}
