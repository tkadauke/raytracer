#pragma once

#include "raytracer/materials/PhongMaterial.h"
#include "raytracer/brdf/PerfectSpecular.h"
#include "raytracer/brdf/PerfectTransmitter.h"

namespace raytracer {
  class TransparentMaterial : public PhongMaterial {
  public:
    inline TransparentMaterial()
      : PhongMaterial()
    {
      setRefractionIndex(1);
    }

    inline TransparentMaterial(Texturec* diffuseTexture)
      : PhongMaterial(diffuseTexture)
    {
      setRefractionIndex(1);
    }

    inline void setRefractionIndex(double index) { m_specularBTDF.setRefractionIndex(index); }
    inline double refractionIndex() const { return m_specularBTDF.refractionIndex(); }

    virtual Colord shade(std::shared_ptr<Raytracer> raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth);

  private:
    Vector3d refract(const Vector3d& direction, const Vector3d& normal, double outerRefractionIndex, double innerRefractionIndex);
    bool totalInternalReflection(const Ray& ray, const HitPoint& hitPoint);

    PerfectSpecular m_reflectiveBRDF;
    PerfectTransmitter m_specularBTDF;
  };
}
