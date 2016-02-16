#pragma once

#include "raytracer/materials/MatteMaterial.h"
#include "raytracer/brdf/GlossySpecular.h"

namespace raytracer {
  class PhongMaterial : public MatteMaterial {
  public:
    inline PhongMaterial()
      : MatteMaterial()
    {
      setSpecularColor(Colord::white());
    }

    inline PhongMaterial(const Colord& diffuse)
      : MatteMaterial(diffuse)
    {
      setSpecularColor(Colord::white());
    }

    inline PhongMaterial(const Colord& diffuse, const Colord& specular)
      : MatteMaterial(diffuse)
    {
      setSpecularColor(specular);
    }

    inline PhongMaterial(const Colord& diffuse, const Colord& specular, double exponent)
      : MatteMaterial(diffuse)
    {
      setSpecularColor(specular);
      setExponent(exponent);
    }

    inline void setSpecularColor(const Colord& color) { m_specularBRDF.setSpecularColor(color); }
    inline const Colord& specularColor() const { return m_specularBRDF.specularColor(); }

    inline void setSpecularCoefficient(double coeff) { m_specularBRDF.setSpecularCoefficient(coeff); }
    inline double specularCoefficient() const { return m_specularBRDF.specularCoefficient(); }

    inline void setExponent(double exponent) { m_specularBRDF.setExponent(exponent); }
    inline double exponent() const { return m_specularBRDF.exponent(); }

    virtual Colord shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth);

  private:
    GlossySpecular m_specularBRDF;
  };
}
