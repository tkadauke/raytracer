#pragma once

#include "world/objects/MatteMaterial.h"

class PhongMaterial : public MatteMaterial {
  Q_OBJECT
  Q_PROPERTY(Colord specularColor READ specularColor WRITE setSpecularColor);
  Q_PROPERTY(double exponent READ exponent WRITE setExponent);

public:
  PhongMaterial(Element* parent = nullptr);

  inline void setSpecularColor(const Colord& color) { m_specularColor = color; }
  inline const Colord& specularColor() const { return m_specularColor; }

  inline void setExponent(double exponent) { m_exponent = exponent; }
  inline double exponent() const { return m_exponent; }

protected:
  virtual raytracer::Material* toRaytracerMaterial() const;
  
private:
  Colord m_specularColor;
  double m_exponent;
};
