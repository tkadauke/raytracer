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

    inline PhongMaterial(Texture<Colord>* diffuseTexture)
      : MatteMaterial(diffuseTexture)
    {
      setSpecularColor(Colord::white());
    }

    inline PhongMaterial(Texture<Colord>* diffuseTexture, const Colord& specular)
      : MatteMaterial(diffuseTexture)
    {
      setSpecularColor(specular);
    }

    inline PhongMaterial(Texture<Colord>* diffuseTexture, const Colord& specular, double exponent)
      : MatteMaterial(diffuseTexture)
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

    virtual Colord shade(std::shared_ptr<Raytracer> raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth);

  private:
    GlossySpecular m_specularBRDF;
  };
}
