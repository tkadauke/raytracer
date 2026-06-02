#pragma once

#include "render/materials/Material.h"
#include "core/math/Matrix.h"
#include "core/math/Ray.h"

namespace render {
  /**
    * Redirects rays through a transformed view of the scene.
    *
    * `PortalMaterial` is evaluated at the surface hit point; it is not a
    * screen-space border or an image pasted onto the rectangle. When a ray
    * hits the portal surface, the material applies the inverse portal
    * transform to the shifted hit-point ray origin and to the ray direction,
    * asks the scene what that transformed ray sees, then multiplies the
    * returned color by the configured filter.
    * Path-tracing integrators see the same redirection as a delta continuation
    * sample because portals change the next ray's origin as well as direction.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="portal_material_ray_redirection.js"></script>
    * @endhtmlonly
    *
    * <table><tr>
    * <td>@image html portal_material__raytracer.png "Raytracer"</td>
    * </tr></table>
    */
  class PortalMaterial : public Material {
  public:
    inline explicit PortalMaterial(const Matrix4d& transformation, const Colord& filter)
        : m_filterColor(filter) {
      setMatrix(transformation);
    }

    void setMatrix(const Matrix4d& matrix);

    Colord shade(const render::RayCaster* raycaster, const render::Scene& scene, const Rayd& ray,
                 const HitPoint& hitPoint, render::State& state) const override;

    bool supportsWhittedContinuations() const override {
      return true;
    }

    bool requiresWhittedPacketHitRefinement() const override {
      return true;
    }

    WhittedShadeResult shadeWhitted(const render::RayCaster* raycaster, const render::Scene& scene,
                                    const Rayd& ray, const HitPoint& hitPoint,
                                    render::State& state) const override;

    bool supportsBsdfSampling() const override {
      return true;
    }

    Colord evalBsdf(const HitPoint& hitPoint, const Vector3d& wi,
                    const Vector3d& wo) const override;

    MaterialBsdfSample sampleBsdf(const HitPoint& hitPoint, const Vector3d& wi,
                                  const Vector2d& sample) const override;

    double bsdfPdf(const HitPoint& hitPoint, const Vector3d& wi, const Vector3d& wo) const override;

  private:
    Rayd redirectedRay(const HitPoint& hitPoint, const Vector3d& wi) const;

    inline Rayd transformedRay(const Rayd& ray) const {
      return Rayd(Vector4d(m_originMatrix.transformPoint(Vector3d(ray.origin()))),
                  m_directionMatrix * ray.direction());
    }

    Matrix4d m_originMatrix;
    Matrix3d m_directionMatrix;
    Colord m_filterColor;
  };
}
