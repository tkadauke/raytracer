#pragma once

#include "raytracer/materials/Material.h"
#include "raytracer/brdf/Lambertian.h"
#include "raytracer/textures/Texture.h"

namespace raytracer {
  /**
    * Matte materials have no reflection, or transmission. As the name suggests,
    * they appear matte.
    * 
    * @image html matte_material_red.png "Matte material with red constant texture"
    */
  class MatteMaterial : public Material {
  public:
    /**
      * Constructs the default matte material with no texture and ambient and
      * diffuse coefficients of 1.
      */
    inline MatteMaterial()
      : Material(),
        m_diffuseTexture(nullptr),
        m_ambientCoefficient(1),
        m_diffuseCoefficient(1)
    {
    }

    /**
      * Constructs the default matte material with the given texture and ambient
      * and diffuse coefficients of 1.
      */
    inline MatteMaterial(std::shared_ptr<Texturec> texture)
      : Material(),
        m_diffuseTexture(texture),
        m_ambientCoefficient(1),
        m_diffuseCoefficient(1)
    {
    }

    /**
      * Sets the material's diffuse texture.
      * 
      * <table><tr>
      * <td>@image html matte_material_rainbow_red.png</td>
      * <td>@image html matte_material_rainbow_orange.png</td>
      * <td>@image html matte_material_rainbow_yellow.png</td>
      * <td>@image html matte_material_rainbow_green.png</td>
      * <td>@image html matte_material_rainbow_blue.png</td>
      * <td>@image html matte_material_rainbow_indigo.png</td>
      * <td>@image html matte_material_rainbow_violet.png</td>
      * </tr></table>
      */
    inline void setDiffuseTexture(std::shared_ptr<Texturec> texture) {
      m_diffuseTexture = texture;
    }
    
    /**
      * @returns the diffuse texture.
      */
    inline std::shared_ptr<Texturec> diffuseTexture() const {
      return m_diffuseTexture;
    }
    
    /**
      * Sets the ambient light coefficient.
      * 
      * <table><tr>
      * <td>@image html matte_material_ambient_0.0.png "ambientCoefficient=0"</td>
      * <td>@image html matte_material_ambient_0.5.png "ambientCoefficient=0.5"</td>
      * <td>@image html matte_material_ambient_1.0.png "ambientCoefficient=1"</td>
      * <td>@image html matte_material_ambient_1.5.png "ambientCoefficient=1.5"</td>
      * <td>@image html matte_material_ambient_2.0.png "ambientCoefficient=2"</td>
      * </tr></table>
      */
    inline void setAmbientCoefficient(double coeff) {
      m_ambientCoefficient = coeff;
    }
    
    /**
      * @returns the ambient light coefficient.
      */
    inline double ambientCoefficient() const {
      return m_ambientCoefficient;
    }
    
    /**
      * Sets the diffuse light coefficient.
      * 
      * <table><tr>
      * <td>@image html matte_material_diffuse_0.0.png "diffuseCoefficient=0"</td>
      * <td>@image html matte_material_diffuse_0.5.png "diffuseCoefficient=0.5"</td>
      * <td>@image html matte_material_diffuse_1.0.png "diffuseCoefficient=1"</td>
      * <td>@image html matte_material_diffuse_1.5.png "diffuseCoefficient=1.5"</td>
      * <td>@image html matte_material_diffuse_2.0.png "diffuseCoefficient=2"</td>
      * </tr></table>
      */
    inline void setDiffuseCoefficient(double coeff) {
      m_diffuseCoefficient = coeff;
    }
    
    /**
      * @returns the diffuse light coefficient.
      */
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
