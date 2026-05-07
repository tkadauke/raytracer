#pragma once

#include "render/materials/Material.h"
#include "core/math/Matrix.h"
#include "core/math/Ray.h"

namespace render {
  class PortalMaterial : public Material {
  public:
    inline explicit PortalMaterial(const Matrix4d& transformation, const Colord& filter)
      : m_filterColor(filter)
    {
      setMatrix(transformation);
    }

    void setMatrix(const Matrix4d& matrix);

    virtual Colord shade(const render::RayCaster* raycaster, const render::Scene& scene, const Rayd& ray, const HitPoint& hitPoint, render::State& state) const;

  private:
    inline Rayd transformedRay(const Rayd& ray) const {
      return Rayd(m_originMatrix * ray.origin(), m_directionMatrix * ray.direction());
    }

    Matrix4d m_originMatrix;
    Matrix3d m_directionMatrix;
    Colord m_filterColor;
  };
}
