#pragma once

#include "raytracer/materials/PhongMaterial.h"
#include "raytracer/brdf/PerfectSpecular.h"
#include "raytracer/brdf/PerfectTransmitter.h"

namespace raytracer {
  /**
    * Transparent materials are used to describe perfectly transparent materials
    * like air, water, glass, or diamonds, that may be tinted on the outside,
    * but don't filter the light while it travels through them. The [refraction
    * index](https://en.wikipedia.org/wiki/Refractive_index) describes how light
    * propagates through the medium.
    * 
    * @image html transparent_material.png "Transparent material"
    */
  class TransparentMaterial : public PhongMaterial {
  public:
    /**
      * Constructs a transparent material with no diffuse texture and a
      * refraction index of 1.
      */
    inline TransparentMaterial()
      : PhongMaterial()
    {
      setRefractionIndex(1);
    }

    /**
      * Constructs a transparent material with diffuseTexture and a refraction
      * index of 1.
      */
    inline TransparentMaterial(std::shared_ptr<Texturec> diffuseTexture)
      : PhongMaterial(diffuseTexture)
    {
      setRefractionIndex(1);
    }

    /**
      * @returns the material's index of refraction
      */
    inline double refractionIndex() const {
      return m_specularBTDF.refractionIndex();
    }
  
    /**
      * Sets the material's [index of refraction](https://en.wikipedia.org/wiki/Refractive_index).
      * 
      * <table><tr>
      * <td>@image html transparent_material_ior_1.01.png "refractionIndex=1.01"</td>
      * <td>@image html transparent_material_ior_1.03.png "refractionIndex=1.03"</td>
      * <td>@image html transparent_material_ior_1.05.png "refractionIndex=1.05"</td>
      * <td>@image html transparent_material_ior_1.07.png "refractionIndex=1.07"</td>
      * <td>@image html transparent_material_ior_1.09.png "refractionIndex=1.09"</td>
      * <td>@image html transparent_material_ior_1.11.png "refractionIndex=1.11"</td>
      * <td>@image html transparent_material_ior_1.13.png "refractionIndex=1.13"</td>
      * </tr></table>
      */
    inline void setRefractionIndex(double index) {
      m_specularBTDF.setRefractionIndex(index);
    }
    
    virtual Colord shade(const Raytracer* raytracer, const Rayd& ray, const HitPoint& hitPoint, State& state) const;

  private:
    Vector3d refract(const Vector3d& direction, const Vector3d& normal, double outerRefractionIndex, double innerRefractionIndex);
    bool totalInternalReflection(const Rayd& ray, const HitPoint& hitPoint);

    PerfectSpecular m_reflectiveBRDF;
    PerfectTransmitter m_specularBTDF;
  };
}
