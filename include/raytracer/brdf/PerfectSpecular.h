#pragma once

#include "raytracer/brdf/BRDF.h"
#include "core/math/Range.h"

namespace raytracer {
  class PerfectSpecular : public BRDF {
  public:
    PerfectSpecular()
      : m_reflectionCoefficient(1)
    {
    }
    
    virtual Colord sample(const HitPoint& hitPoint, const Vector3d& out, Vector3d& in);
    
    inline void setReflectionColor(const Colord& color) { m_reflectionColor = color; }
    inline const Colord& reflectionColor() const { return m_reflectionColor; }
    
    inline void setReflectionCoefficient(double coeff) {
      m_reflectionCoefficient = Ranged(0, 1).clamp(coeff);
    }
    
    inline double reflectionCoefficient() const { return m_reflectionCoefficient; }
    
  private:
    Colord m_reflectionColor;
    double m_reflectionCoefficient;
  };
}
