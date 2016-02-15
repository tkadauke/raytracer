#pragma once

#include "raytracer/materials/Material.h"
#include "raytracer/brdf/Lambertian.h"

namespace raytracer {
  class MatteMaterial : public Material {
  public:
    inline MatteMaterial() : Material() {}

    inline MatteMaterial(const Colord& color)
      : Material()
    {
      setDiffuseColor(color);
    }

    inline void setDiffuseColor(const Colord& color) {
      m_ambientBRDF.setDiffuseColor(color);
      m_diffuseBRDF.setDiffuseColor(color);
    }

    inline const Colord& diffuseColor() const {
      return m_ambientBRDF.diffuseColor();
    }
    
    inline void setAmbientCoefficient(double coeff) { m_ambientBRDF.setReflectionCoefficient(coeff); }
    inline double ambientCoefficient() const { return m_ambientBRDF.reflectionCoefficient(); }
    inline void setDiffuseCoefficient(double coeff) { m_diffuseBRDF.setReflectionCoefficient(coeff); }
    inline double diffuseCoefficient() const { return m_diffuseBRDF.reflectionCoefficient(); }

    virtual Colord shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth);

  private:
    Lambertian m_ambientBRDF;
    Lambertian m_diffuseBRDF;
  };
}
