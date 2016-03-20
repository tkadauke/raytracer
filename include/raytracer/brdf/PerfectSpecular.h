#pragma once

#include "raytracer/brdf/BRDF.h"
#include "core/math/Range.h"

namespace raytracer {
  class PerfectSpecular : public BRDF {
  public:
    inline PerfectSpecular()
      : m_reflectionCoefficient(1)
    {
    }
    
    virtual Colord sample(const HitPoint& hitPoint, const Vector3d& out, Vector3d& in) const;
    
    inline const Colord& reflectionColor() const {
      return m_reflectionColor;
    }
    
    inline void setReflectionColor(const Colord& color) {
      m_reflectionColor = color;
    }
    
    inline double reflectionCoefficient() const {
      return m_reflectionCoefficient;
    }
    
    inline void setReflectionCoefficient(double coeff) {
      m_reflectionCoefficient = Ranged(0, 1).clamp(coeff);
    }
    
  private:
    Colord m_reflectionColor;
    double m_reflectionCoefficient;
  };
}
