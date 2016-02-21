#pragma once

#include "raytracer/materials/Material.h"
#include "raytracer/brdf/Lambertian.h"

namespace raytracer {
  template<class T>
  class Texture;
  
  class MatteMaterial : public Material {
  public:
    inline MatteMaterial()
      : Material(), m_texture(nullptr), m_ambientCoefficient(1), m_diffuseCoefficient(1)
    {
    }

    inline MatteMaterial(Texture<Colord>* texture)
      : Material(), m_texture(texture), m_ambientCoefficient(1), m_diffuseCoefficient(1)
    {
    }

    inline void setDiffuseTexture(Texture<Colord>* texture) {
      m_texture = texture;
    }

    inline Texture<Colord>* diffuseTexture() const {
      return m_texture;
    }
    
    inline void setAmbientCoefficient(double coeff) { m_ambientCoefficient = coeff; }
    inline double ambientCoefficient() const { return m_ambientCoefficient; }
    inline void setDiffuseCoefficient(double coeff) { m_diffuseCoefficient = coeff; }
    inline double diffuseCoefficient() const { return m_diffuseCoefficient; }

    virtual Colord shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth);

  protected:
    Texture<Colord>* m_texture;
    double m_ambientCoefficient;
    double m_diffuseCoefficient;
  };
}
