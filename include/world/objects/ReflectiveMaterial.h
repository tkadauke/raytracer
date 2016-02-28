#pragma once

#include "world/objects/PhongMaterial.h"

class ReflectiveMaterial : public PhongMaterial {
  Q_OBJECT;
  Q_PROPERTY(Colord reflectionColor READ reflectionColor WRITE setReflectionColor);
  Q_PROPERTY(double reflectionCoefficient READ reflectionCoefficient WRITE setReflectionCoefficient);

public:
  ReflectiveMaterial(Element* parent = nullptr);

  inline void setReflectionColor(const Colord& color) {
    m_reflectionColor = color;
  }
  
  inline const Colord& reflectionColor() const {
    return m_reflectionColor;
  }
  
  inline void setReflectionCoefficient(double coeff) {
    m_reflectionCoefficient = coeff;
  }
  
  inline double reflectionCoefficient() const {
    return m_reflectionCoefficient;
  }
  
protected:
  virtual raytracer::Material* toRaytracerMaterial() const;
  
private:
  Colord m_reflectionColor;
  double m_reflectionCoefficient;
};
