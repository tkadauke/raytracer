#pragma once

#include "world/objects/PhongMaterial.h"

class TransparentMaterial : public PhongMaterial {
  Q_OBJECT
  Q_PROPERTY(Colord absorbanceColor READ absorbanceColor WRITE setAbsorbanceColor);
  Q_PROPERTY(double refractionIndex READ refractionIndex WRITE setRefractionIndex);

public:
  TransparentMaterial(Element* parent = nullptr);

  inline void setAbsorbanceColor(const Colord& color) { m_absorbanceColor = color; }
  inline const Colord& absorbanceColor() const { return m_absorbanceColor; }

  inline void setRefractionIndex(double index) { m_refractionIndex = index; }
  inline double refractionIndex() const { return m_refractionIndex; }

protected:
  virtual raytracer::Material* toRaytracerMaterial() const;

private:
  Colord m_absorbanceColor;
  double m_refractionIndex;
};
