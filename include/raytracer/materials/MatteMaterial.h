#pragma once

#include "raytracer/materials/Material.h"
#include "raytracer/brdf/Lambertian.h"
#include "raytracer/textures/Texture.h"

namespace raytracer {
  class MatteMaterial : public Material {
  public:
    inline MatteMaterial()
      : Material(),
        m_diffuseTexture(nullptr),
        m_ambientCoefficient(1),
        m_diffuseCoefficient(1)
    {
    }

    inline MatteMaterial(std::shared_ptr<Texturec> texture)
      : Material(),
        m_diffuseTexture(texture),
        m_ambientCoefficient(1),
        m_diffuseCoefficient(1)
    {
    }

    inline void setDiffuseTexture(std::shared_ptr<Texturec> texture) {
      m_diffuseTexture = texture;
    }

    inline std::shared_ptr<Texturec> diffuseTexture() const {
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
    
    virtual Colord shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth);

  private:
    std::shared_ptr<Texturec> m_diffuseTexture;
    double m_ambientCoefficient;
    double m_diffuseCoefficient;
  };
}
