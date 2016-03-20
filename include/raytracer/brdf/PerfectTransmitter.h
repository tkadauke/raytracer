#pragma once

#include "raytracer/brdf/BTDF.h"
#include "core/math/Range.h"

namespace raytracer {
  class PerfectTransmitter : public BTDF {
  public:
    inline PerfectTransmitter()
      : m_transmissionCoefficient(1),
        m_refractionIndex(16)
    {
    }
    
    virtual Colord sample(const HitPoint& hitPoint, const Vector3d& out, Vector3d& in) const;
    virtual bool totalInternalReflection(const Rayd& ray, const HitPoint& hitPoint) const;
    
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
