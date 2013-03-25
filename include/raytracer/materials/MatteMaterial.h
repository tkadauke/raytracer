#ifndef MATTE_MATERIAL_H
#define MATTE_MATERIAL_H

#include "raytracer/materials/Material.h"

class MatteMaterial : public Material {
public:
  inline MatteMaterial() : Material() {}
  
  inline MatteMaterial(const Colord& color)
    : Material(), m_diffuseColor(color)
  {
  }
  
  inline void setDiffuseColor(const Colord& color) { m_diffuseColor = color; }
  inline const Colord& diffuseColor() const { return m_diffuseColor; }

  virtual Colord shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth);

private:
  Colord m_diffuseColor;
};

#endif
