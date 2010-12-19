#ifndef REFLECTIVE_MATERIAL_H
#define REFLECTIVE_MATERIAL_H

#include "materials/PhongMaterial.h"

class ReflectiveMaterial : public PhongMaterial {
public:
  inline ReflectiveMaterial()
    : PhongMaterial()
  {
  }
  
  inline ReflectiveMaterial(const Colord& color)
    : PhongMaterial(color)
  {
  }
  
  inline ReflectiveMaterial(const Colord& diffuse, const Colord& highlight)
    : PhongMaterial(diffuse, highlight)
  {
  }
  
  inline ReflectiveMaterial(const Colord& diffuse, const Colord& highlight, const Colord& specular)
    : PhongMaterial(diffuse, highlight), m_specularColor(specular)
  {
  }
  
  inline void setSpecularColor(const Colord& color) { m_specularColor = color; }
  inline const Colord& specularColor() const { return m_specularColor; }
  
  virtual Colord shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth);

private:
  Colord m_specularColor;
};

#endif
