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

    inline TransparentMaterial(std::shared_ptr<Texturec> diffuseTexture)
      : PhongMaterial(diffuseTexture)
    {
      setRefractionIndex(1);
    }

    /**
      * Sets the material's index of refraction.
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
    
    /**
      * @returns the material's index of refraction
      */
    inline double refractionIndex() const {
      return m_specularBTDF.refractionIndex();
    }
    
    virtual Colord shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth);

  private:
    Vector3d refract(const Vector3d& direction, const Vector3d& normal, double outerRefractionIndex, double innerRefractionIndex);
    bool totalInternalReflection(const Ray& ray, const HitPoint& hitPoint);

    PerfectSpecular m_reflectiveBRDF;
    PerfectTransmitter m_specularBTDF;
  };
}
