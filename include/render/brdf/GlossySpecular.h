#pragma once

#include "render/brdf/BRDF.h"
#include "core/math/Range.h"

namespace render {
  /**
    * GlossySpecular evaluates the Phong specular lobe around the reflected
    * light direction. The highlight is visible only when the outgoing view
    * vector points near that lobe; increasing the exponent raises the
    * alignment term to a higher power, narrowing the highlight.
    *
    * The widget shows the fixed surface normal, draggable light and view
    * vectors, the Lambertian `n dot l` term, and the Phong lobe controlled by
    * the specular coefficient and exponent.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="phong_lambertian_lobes.js"></script>
    * @endhtmlonly
    */
  class GlossySpecular : public BRDF {
  public:
    inline GlossySpecular()
        : m_specularCoefficient(1),
          m_exponent(16) {
    }

    Colord calculate(const HitPoint& hitPoint, const Vector3d& out,
                     const Vector3d& in) const override;

    int flags() const override {
      return BSDF::Glossy | BSDF::Reflection;
    }

    inline const Colord& specularColor() const {
      return m_specularColor;
    }

    inline void setSpecularColor(const Colord& color) {
      m_specularColor = color;
    }

    inline double specularCoefficient() const {
      return m_specularCoefficient;
    }

    inline void setSpecularCoefficient(double coeff) {
      m_specularCoefficient = Ranged(0, 1).clamp(coeff);
    }

    inline double exponent() const {
      return m_exponent;
    }

    inline void setExponent(double exponent) {
      m_exponent = exponent;
    }

  private:
    Colord m_specularColor;
    double m_specularCoefficient;
    double m_exponent;
  };
}
