#pragma once

#include "render/brdf/BTDF.h"
#include "core/math/Range.h"

namespace render {
  class PerfectTransmitter : public BTDF {
  public:
    inline PerfectTransmitter()
      : m_transmissionCoefficient(1),
        m_refractionIndex(16)
    {
    }
    
    Colord sample(const HitPoint& hitPoint, const Vector3d& out, Vector3d& in) const override;
    bool totalInternalReflection(const Rayd& ray, const HitPoint& hitPoint) const override;

    /// BSDF::sample for the delta-transmission lobe — sets
    /// `pdf = 1` to flag the delta, delegates to the geometric
    /// `sample(hp, out, in)` above. Note: this does NOT branch on
    /// TIR; that decision lives in `TransparentMaterial::shade`,
    /// which inspects `totalInternalReflection` first.
    Colord sample(const HitPoint& hitPoint, const Vector3d& wi, Vector3d& wo, double& pdf) const override {
      pdf = 1.0;
      return sample(hitPoint, wi, wo);
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
