#pragma once

#include "raytracer/materials/PhongMaterial.h"
#include "raytracer/brdf/PerfectSpecular.h"

namespace raytracer {
  class ReflectiveMaterial : public PhongMaterial {
  public:
    inline ReflectiveMaterial()
      : PhongMaterial()
    {
      setReflectionCoefficient(0.75);
      setReflectionColor(Colord::white());
    }

    inline ReflectiveMaterial(std::shared_ptr<Texturec> diffuseTexture)
      : PhongMaterial(diffuseTexture)
    {
      setReflectionCoefficient(0.75);
      setReflectionColor(Colord::white());
    }

    inline ReflectiveMaterial(std::shared_ptr<Texturec> diffuseTexture, const Colord& specular)
      : PhongMaterial(diffuseTexture, specular)
    {
      setReflectionCoefficient(0.75);
      setReflectionColor(Colord::white());
    }

    virtual Colord shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth);
    
    inline void setReflectionColor(const Colord& color) { m_reflectiveBRDF.setReflectionColor(color); }
    inline const Colord& reflectionColor() const { return m_reflectiveBRDF.reflectionColor(); }
    
    inline void setReflectionCoefficient(double coeff) { m_reflectiveBRDF.setReflectionCoefficient(coeff); }
    inline double reflectionCoefficient() const { return m_reflectiveBRDF.reflectionCoefficient(); }
    
  protected:
    PerfectSpecular m_reflectiveBRDF;
  };
}
