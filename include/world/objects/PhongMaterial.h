#pragma once

#include "world/objects/MatteMaterial.h"

class PhongMaterial : public MatteMaterial {
  Q_OBJECT
  Q_PROPERTY(Colord highlightColor READ highlightColor WRITE setHighlightColor);
  Q_PROPERTY(double exponent READ exponent WRITE setExponent);

public:
  PhongMaterial(Element* parent = nullptr);

  inline void setHighlightColor(const Colord& color) { m_highlightColor = color; }
  inline const Colord& highlightColor() const { return m_highlightColor; }

  inline void setExponent(double exponent) { m_exponent = exponent; }
  inline double exponent() const { return m_exponent; }

protected:
  virtual raytracer::Material* toRaytracerMaterial() const;
  
private:
  Colord m_highlightColor;
  double m_exponent;
};
