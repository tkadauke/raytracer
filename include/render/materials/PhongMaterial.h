#pragma once
#include <memory>

#include "render/materials/MatteMaterial.h"
#include "render/brdf/GlossySpecular.h"

namespace render {
  /**
    * Phong materials are used to shade physically incorrect, but easy-to-compute
    * shiny surfaces. The reflection model used in this material is the
    * [Phong reflection model](https://en.wikipedia.org/wiki/Phong_reflection_model).
    * It is a combination of ambient and diffuse shading, with the addition of
    * small intense specular highlights.
    * 
    * Same Phong sphere through both engines. The raytracer and
    * software rasterizer both apply the local Phong model:
    * Lambertian diffuse plus the sharp specular highlight where the
    * view direction reflects off the surface toward the light.
    *
    * The specular highlight is view dependent. The Phong exponent
    * narrows the reflected-light lobe: low values make broad, soft
    * highlights, while high values leave only view vectors close to
    * the mirror-reflection direction.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="phong_lambertian_lobes.js"></script>
    * @endhtmlonly
    *
    * <table>
    *   <tr>
    *     <th>Raytracer</th>
    *     <th>Software rasterizer</th>
    *   </tr>
    *   <tr>
    *     <td>@image html phong_material_red__raytracer.png ""</td>
    *     <td>@image html phong_material_red__raster.png ""</td>
    *   </tr>
    * </table>
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
    inline explicit PhongMaterial(std::shared_ptr<render::Texturec> diffuseTexture)
      : MatteMaterial(diffuseTexture)
    {
      setSpecularColor(Colord::white());
    }
    
    /**
      * Constructs a Phong material with diffuseTexture and specularColor.
      */
    inline explicit PhongMaterial(std::shared_ptr<render::Texturec> diffuseTexture, const Colord& specular)
      : MatteMaterial(diffuseTexture)
    {
      setSpecularColor(specular);
    }
    
    /**
      * Constructs a Phong material with diffuseTexture and specularColor and
      * the given Phong exponent.
      */
    inline explicit PhongMaterial(std::shared_ptr<render::Texturec> diffuseTexture, const Colord& specular, double exponent)
      : MatteMaterial(diffuseTexture)
    {
      setSpecularColor(specular);
      setExponent(exponent);
    }
    
    /**
      * @returns the material's specular color.
      */
    inline const Colord& specularColor() const {
      return m_specularBRDF.specularColor();
    }
    
    /**
      * Sets the material's specular color.
      * 
      * <table><tr>
      * <td>@image html phong_material_specular_color_red.png "red"</td>
      * <td>@image html phong_material_specular_color_orange.png "orange"</td>
      * <td>@image html phong_material_specular_color_yellow.png "yellow"</td>
      * <td>@image html phong_material_specular_color_green.png "green"</td>
      * <td>@image html phong_material_specular_color_blue.png "blue"</td>
      * <td>@image html phong_material_specular_color_indigo.png "indigo"</td>
      * <td>@image html phong_material_specular_color_violet.png "violet"</td>
      * </tr></table>
      */
    inline void setSpecularColor(const Colord& color) {
      m_specularBRDF.setSpecularColor(color);
    }
    
    /**
      * @returns the material's specular reflection coefficient.
      */
    inline double specularCoefficient() const {
      return m_specularBRDF.specularCoefficient();
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
      * @returns the material's lobe exponent.
      */
    inline double exponent() const {
      return m_specularBRDF.exponent();
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
    
    virtual Colord shade(const render::RayCaster* raycaster, const render::Scene& scene, const Rayd& ray, const HitPoint& hitPoint, render::State& state) const;

  private:
    render::GlossySpecular m_specularBRDF;
  };
}
