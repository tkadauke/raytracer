#pragma once

#include "render/brdf/BTDF.h"
#include "core/math/Range.h"

namespace render {
  /**
    * Perfect delta-transmission BTDF used by TransparentMaterial. It computes
    * the single refracted direction from Snell's law and reports total
    * internal reflection when the requested transmission angle has no real
    * solution.
    *
    * The widget below shows the same geometric branch used by
    * `sample()` and `totalInternalReflection()`: rays inside a higher-IOR
    * medium bend away from the normal as they exit, and beyond the critical
    * angle the transmitted branch disappears entirely.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="transparent_material_refraction.js"></script>
    * @endhtmlonly
    */
  class PerfectTransmitter : public BTDF {
  public:
    inline PerfectTransmitter()
        : m_transmissionCoefficient(1),
          m_refractionIndex(16) {
    }

    Colord sample(const HitPoint& hitPoint, const Vector3d& out, Vector3d& in) const override;
    using BTDF::sample;
    bool totalInternalReflection(const Rayd& ray, const HitPoint& hitPoint) const override;

    /// BSDF::sample for the delta-transmission lobe — sets
    /// `pdf = 1` to flag the delta, delegates to the geometric
    /// `sample(hp, out, in)` above. Note: this does NOT branch on
    /// TIR; that decision lives in `TransparentMaterial::shade`,
    /// which inspects `totalInternalReflection` first.
    Colord sample(const HitPoint& hitPoint, const Vector3d& wi, Vector3d& wo,
                  double& pdf) const override {
      pdf = 1.0;
      return sample(hitPoint, wi, wo);
    }
    Colord sample(const HitPoint& hitPoint, const Vector3d& wi, Vector3d& wo, double& pdf,
                  const Vector2d&) const override {
      return sample(hitPoint, wi, wo, pdf);
    }

    inline double transmissionCoefficient() const {
      return m_transmissionCoefficient;
    }

    inline void setTransmissionCoefficient(double coeff) {
      m_transmissionCoefficient = Ranged(0, 1).clamp(coeff);
    }

    inline double refractionIndex() const {
      return m_refractionIndex;
    }

    inline void setRefractionIndex(double refractionIndex) {
      m_refractionIndex = refractionIndex;
    }

  private:
    double m_transmissionCoefficient;
    double m_refractionIndex;
  };
}
