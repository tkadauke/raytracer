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
