#include "render/materials/EmissiveMaterial.h"

#include "core/math/HitPoint.h"

using namespace render;

EmissiveMaterial::EmissiveMaterial(const Colord& radiance)
    : m_radiance(radiance) {
}

const Colord& EmissiveMaterial::radiance() const {
  return m_radiance;
}

Colord EmissiveMaterial::shade(const render::RayCaster*, const render::Scene&, const Rayd& ray,
                               const HitPoint& hitPoint, render::State&) const {
  return emittedRadiance(ray, hitPoint);
}

Colord EmissiveMaterial::emittedRadiance(const Rayd& ray, const HitPoint& hitPoint) const {
  return emitsToward(ray, hitPoint) ? radiance() : Colord::black();
}

bool EmissiveMaterial::supportsBsdfSampling() const {
  return true;
}

bool EmissiveMaterial::emitsToward(const Rayd& ray, const HitPoint& hitPoint) const {
  return hitPoint.normal() * -ray.direction().normalized() > emissionTolerance;
}
