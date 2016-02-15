#pragma once

#include "world/objects/Material.h"
#include "core/Color.h"

class MatteMaterial : public Material {
  Q_OBJECT
  Q_PROPERTY(Colord diffuseColor READ diffuseColor WRITE setDiffuseColor);

public:
  MatteMaterial(Element* parent = nullptr);

  inline void setDiffuseColor(const Colord& color) { m_diffuseColor = color; }
  inline const Colord& diffuseColor() const { return m_diffuseColor; }

protected:
  virtual raytracer::Material* toRaytracerMaterial() const;
  
private:
  Colord m_diffuseColor;
};
