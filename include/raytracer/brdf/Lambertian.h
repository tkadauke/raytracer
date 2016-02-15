#pragma once

#include "raytracer/brdf/BRDF.h"
#include "core/math/Range.h"

namespace raytracer {
  class Lambertian : public BRDF {
  public:
    Lambertian()
      : m_reflectionCoefficient(1)
    {
    }
    
    virtual Colord calculate(const HitPoint& hitPoint, const Vector3d& in, const Vector3d& out);
    virtual Colord reflectance(const HitPoint& hitPoint, const Vector3d& out);
    
    inline void setDiffuseColor(const Colord& color) { m_diffuseColor = color; }
    inline const Colord& diffuseColor() const { return m_diffuseColor; }
    
    inline void setReflectionCoefficient(double coeff) {
      m_reflectionCoefficient = Ranged(0, 1).clamp(coeff);
    }
    
    inline double reflectionCoefficient() const { return m_reflectionCoefficient; }
    
  private:
    Colord m_diffuseColor;
    double m_reflectionCoefficient;
  };
}
