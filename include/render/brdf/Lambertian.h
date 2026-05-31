#pragma once

#include "render/brdf/BRDF.h"
#include "core/math/Range.h"

namespace render {
  /**
    * Lambertian diffuse reflection is view independent. The BRDF returns a
    * constant color term, while the direct-lighting code multiplies it by the
    * geometric cosine factor `max(n dot l, 0)`: light at grazing angles
    * contributes less than light aligned with the surface normal.
    *
    * The widget compares that cosine-weighted diffuse term with the
    * view-dependent Phong specular lobe used by GlossySpecular.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="phong_lambertian_lobes.js"></script>
    * @endhtmlonly
    */
  class Lambertian : public BRDF {
  public:
    inline Lambertian()
        : m_reflectionCoefficient(1) {
    }

    inline explicit Lambertian(const Colord& color, double coeff)
        : m_diffuseColor(color),
          m_reflectionCoefficient(coeff) {
    }

    Colord calculate(const HitPoint& hitPoint, const Vector3d& out,
                     const Vector3d& in) const override;
    Colord reflectance(const HitPoint& hitPoint, const Vector3d& out) const override;
    using BRDF::sample;
    Colord sample(const HitPoint& hitPoint, const Vector3d& wi, Vector3d& wo, double& pdf,
                  const Vector2d& sample) const override;
    double pdf(const HitPoint& hitPoint, const Vector3d& wi, const Vector3d& wo) const override;

    int flags() const override {
      return BSDF::Diffuse | BSDF::Reflection;
    }

    inline const Colord& diffuseColor() const {
      return m_diffuseColor;
    }

    inline void setDiffuseColor(const Colord& color) {
      m_diffuseColor = color;
    }

    inline double reflectionCoefficient() const {
      return m_reflectionCoefficient;
    }

    inline void setReflectionCoefficient(double coeff) {
      m_reflectionCoefficient = Ranged(0, 1).clamp(coeff);
    }

  private:
    Colord m_diffuseColor;
    double m_reflectionCoefficient;
  };
}
