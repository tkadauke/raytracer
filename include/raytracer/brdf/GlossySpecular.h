#pragma once

#include "raytracer/brdf/BRDF.h"
#include "core/math/Range.h"

namespace raytracer {
  class GlossySpecular : public BRDF {
  public:
    inline GlossySpecular()
      : m_specularCoefficient(1),
        m_exponent(16)
    {
    }
    
    virtual Colord calculate(const HitPoint& hitPoint, const Vector3d& out, const Vector3d& in);
    
    inline void setSpecularColor(const Colord& color) {
      m_specularColor = color;
    }
    
    inline const Colord& specularColor() const {
      return m_specularColor;
    }
    
    inline void setSpecularCoefficient(double coeff) {
      m_specularCoefficient = Ranged(0, 1).clamp(coeff);
    }
    
    inline double specularCoefficient() const {
      return m_specularCoefficient;
    }
    
    inline void setExponent(double exponent) {
      m_exponent = exponent;
    }
    
    inline double exponent() const {
      return m_exponent;
    }
    
  private:
    Colord m_specularColor;
    double m_specularCoefficient;
    double m_exponent;
  };
}
