#pragma once

#include "render/materials/Material.h"

namespace render {
  /**
    * One-sided constant-emission material used by finite light geometry.
    *
    * The material emits only in the hemisphere pointed to by the hit normal.
    * It exposes no reflective or transmissive BSDF lobe; path tracers add its
    * emitted radiance when a path hits it and then terminate that path.
    */
  class EmissiveMaterial : public Material {
  public:
    explicit EmissiveMaterial(const Colord& radiance);

    const Colord& radiance() const;

    Colord shade(const render::RayCaster* raycaster, const render::Scene& scene, const Rayd& ray,
                 const HitPoint& hitPoint, render::State& state) const override;

    Colord emittedRadiance(const Rayd& ray, const HitPoint& hitPoint) const override;

    bool supportsPathTracing() const override;

  private:
    static constexpr double emissionTolerance = 1e-9;

    bool emitsToward(const Rayd& ray, const HitPoint& hitPoint) const;

    Colord m_radiance;
  };
}
