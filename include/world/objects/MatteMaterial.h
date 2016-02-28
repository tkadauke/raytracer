#pragma once

#include "world/objects/Material.h"

class Texture;

class MatteMaterial : public Material {
  Q_OBJECT;
  Q_PROPERTY(Texture* diffuseTexture READ diffuseTexture WRITE setDiffuseTexture);
  Q_PROPERTY(double ambientCoefficient READ ambientCoefficient WRITE setAmbientCoefficient);
  Q_PROPERTY(double diffuseCoefficient READ diffuseCoefficient WRITE setDiffuseCoefficient);

public:
  MatteMaterial(Element* parent = nullptr);

  inline void setDiffuseTexture(Texture* texture) {
    m_diffuseTexture = texture;
  }
  
  inline Texture* diffuseTexture() const {
    return m_diffuseTexture;
  }
  
  inline void setAmbientCoefficient(double coeff) {
    m_ambientCoefficient = coeff;
  }
  
  inline double ambientCoefficient() const {
    return m_ambientCoefficient;
  }
  
  inline void setDiffuseCoefficient(double coeff) {
    m_diffuseCoefficient = coeff;
  }
  
  inline double diffuseCoefficient() const {
    return m_diffuseCoefficient;
  }
  
protected:
  virtual raytracer::Material* toRaytracerMaterial() const;
  
private:
  Texture* m_diffuseTexture;
  double m_ambientCoefficient;
  double m_diffuseCoefficient;
};
