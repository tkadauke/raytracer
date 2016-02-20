#pragma once

#include "world/objects/PhongMaterial.h"

class TransparentMaterial : public PhongMaterial {
  Q_OBJECT
  Q_PROPERTY(double refractionIndex READ refractionIndex WRITE setRefractionIndex);

public:
  TransparentMaterial(Element* parent = nullptr);

  inline void setRefractionIndex(double index) { m_refractionIndex = index; }
  inline double refractionIndex() const { return m_refractionIndex; }

protected:
  virtual raytracer::Material* toRaytracerMaterial() const;

private:
  double m_refractionIndex;
};
