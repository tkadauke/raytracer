#pragma once

#include "world/objects/Material.h"
#include "core/Color.h"

class MatteMaterial : public Material {
  Q_OBJECT
  Q_PROPERTY(Colord diffuseColor READ diffuseColor WRITE setDiffuseColor);
  Q_PROPERTY(double ambientCoefficient READ ambientCoefficient WRITE setAmbientCoefficient);
  Q_PROPERTY(double diffuseCoefficient READ diffuseCoefficient WRITE setDiffuseCoefficient);

public:
  MatteMaterial(Element* parent = nullptr);

  inline void setDiffuseColor(const Colord& color) { m_diffuseColor = color; }
  inline const Colord& diffuseColor() const { return m_diffuseColor; }

  inline void setAmbientCoefficient(double coeff) { m_ambientCoefficient = coeff; }
  inline double ambientCoefficient() const { return m_ambientCoefficient; }

  inline void setDiffuseCoefficient(double coeff) { m_diffuseCoefficient = coeff; }
  inline double diffuseCoefficient() const { return m_diffuseCoefficient; }

protected:
  virtual raytracer::Material* toRaytracerMaterial() const;
  
private:
  Colord m_diffuseColor;
  double m_ambientCoefficient;
  double m_diffuseCoefficient;
};
