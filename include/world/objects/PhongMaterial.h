#pragma once

#include "world/objects/MatteMaterial.h"
#include "core/math/Range.h"

class PhongMaterial : public MatteMaterial {
  Q_OBJECT
  Q_PROPERTY(Colord specularColor READ specularColor WRITE setSpecularColor);
  Q_PROPERTY(double exponent READ exponent WRITE setExponent);
  Q_PROPERTY(double specularCoefficient READ specularCoefficient WRITE setSpecularCoefficient);

public:
  PhongMaterial(Element* parent = nullptr);

  inline void setSpecularColor(const Colord& color) { m_specularColor = color; }
  inline const Colord& specularColor() const { return m_specularColor; }

  inline void setExponent(double exponent) { m_exponent = exponent; }
  inline double exponent() const { return m_exponent; }

  inline void setSpecularCoefficient(double coeff) {
    m_specularCoefficient = Ranged(0, 1).clamp(coeff);
  }
  inline double specularCoefficient() const { return m_specularCoefficient; }

protected:
  virtual raytracer::Material* toRaytracerMaterial() const;
  
private:
  Colord m_specularColor;
  double m_exponent;
  double m_specularCoefficient;
};
