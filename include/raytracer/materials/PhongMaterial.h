#pragma once

#include "raytracer/materials/MatteMaterial.h"
#include "raytracer/brdf/GlossySpecular.h"

namespace raytracer {
  /**
    * Phong materials are used to shade physically incorrect, but easy-to-compute
    * shiny surfaces. The reflection model used in this material is the
    * [Phong reflection model](https://en.wikipedia.org/wiki/Phong_reflection_model).
    * It is a combination of ambient and diffuse shading, with the addition of
    * small intense specular highlights.
    * 
    * @image html phong_material_red.png "Phong material"
    */
  class PhongMaterial : public MatteMaterial {
  public:
    /**
      * Constructs a default Phong material with no diffuse texture and a white
      * specular color.
      */
    inline PhongMaterial()
      : MatteMaterial()
    {
      setSpecularColor(Colord::white());
    }
    
    /**
      * Constructs a Phong material with diffuseTexture and a white specular
      * color.
      */
    inline PhongMaterial(std::shared_ptr<Texturec> diffuseTexture)
      : MatteMaterial(diffuseTexture)
    {
      setSpecularColor(Colord::white());
    }
    
    /**
      * Constructs a Phong material with diffuseTexture and specularColor.
      */
    inline PhongMaterial(std::shared_ptr<Texturec> diffuseTexture, const Colord& specular)
      : MatteMaterial(diffuseTexture)
    {
      setSpecularColor(specular);
    }
    
    /**
      * Constructs a Phong material with diffuseTexture and specularColor and
      * the given Phong exponent.
      */
    inline PhongMaterial(std::shared_ptr<Texturec> diffuseTexture, const Colord& specular, double exponent)
      : MatteMaterial(diffuseTexture)
    {
      setSpecularColor(specular);
      setExponent(exponent);
    }
    
    /**
      * Sets the material's specular color.
      * 
      * <table><tr>
      * <td>@image html phong_material_specular_color_red.png</td>
      * <td>@image html phong_material_specular_color_orange.png</td>
      * <td>@image html phong_material_specular_color_yellow.png</td>
      * <td>@image html phong_material_specular_color_green.png</td>
      * <td>@image html phong_material_specular_color_blue.png</td>
      * <td>@image html phong_material_specular_color_indigo.png</td>
      * <td>@image html phong_material_specular_color_violet.png</td>
      * </tr></table>
      */
    inline void setSpecularColor(const Colord& color) {
      m_specularBRDF.setSpecularColor(color);
    }
    
    /**
      * @returns the material's specular color.
      */
    inline const Colord& specularColor() const {
      return m_specularBRDF.specularColor();
    }
    
    /**
      * Sets the specular reflection coefficient.
      * 
      * <table><tr>
      * <td>@image html phong_material_specular_coeff_0.0.png "specularCoefficient=0"</td>
      * <td>@image html phong_material_specular_coeff_0.25.png "specularCoefficient=0.25"</td>
      * <td>@image html phong_material_specular_coeff_0.5.png "specularCoefficient=0.5"</td>
      * <td>@image html phong_material_specular_coeff_0.75.png "specularCoefficient=0.75"</td>
      * <td>@image html phong_material_specular_coeff_1.0.png "specularCoefficient=1"</td>
      * </tr></table>
      */
    inline void setSpecularCoefficient(double coeff) {
      m_specularBRDF.setSpecularCoefficient(coeff);
    }
    
    /**
      * @returns the material's specular reflection coefficient.
      */
    inline double specularCoefficient() const {
      return m_specularBRDF.specularCoefficient();
    }
    
    /**
      * Sets the specular lobe exponent.
      * 
      * <table><tr>
      * <td>@image html phong_material_exponent_1.png "exponent=1"</td>
      * <td>@image html phong_material_exponent_8.png "exponent=8"</td>
      * <td>@image html phong_material_exponent_27.png "exponent=27"</td>
      * <td>@image html phong_material_exponent_64.png "exponent=64"</td>
      * <td>@image html phong_material_exponent_125.png "exponent=125"</td>
      * </tr></table>
      */
    inline void setExponent(double exponent) {
      m_specularBRDF.setExponent(exponent);
    }
    
    /**
      * @returns the material's lobe exponent.
      */
    inline double exponent() const {
      return m_specularBRDF.exponent();
    }
    
    virtual Colord shade(Raytracer* raytracer, const Rayd& ray, const HitPoint& hitPoint, State& state);

  private:
    GlossySpecular m_specularBRDF;
  };
}
