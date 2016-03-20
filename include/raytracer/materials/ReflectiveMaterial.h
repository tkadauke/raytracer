#pragma once

#include "raytracer/materials/PhongMaterial.h"
#include "raytracer/brdf/PerfectSpecular.h"

namespace raytracer {
  /**
    * Reflective materials describe shiny objects like polished metal or
    * mirrors.
    * 
    * @image html reflective_material_red.png "Reflective material"
    */
  class ReflectiveMaterial : public PhongMaterial {
  public:
    /**
      * Constructs a default reflective material with no diffuse texture, a
      * reflection coefficient of 0.75 and a white reflection color.
      */
    inline ReflectiveMaterial()
      : PhongMaterial()
    {
      setReflectionCoefficient(0.75);
      setReflectionColor(Colord::white());
    }
    
    /**
      * Constructs a reflective material with diffuseTexture, a reflection
      * coefficient of 0.75 and a white reflection color.
      */
    inline explicit ReflectiveMaterial(std::shared_ptr<Texturec> diffuseTexture)
      : PhongMaterial(diffuseTexture)
    {
      setReflectionCoefficient(0.75);
      setReflectionColor(Colord::white());
    }

    /**
      * Constructs a reflective material with diffuseTexture, the given specular
      * color, a reflection coefficient of 0.75 and a white reflection color.
      */
    inline explicit ReflectiveMaterial(std::shared_ptr<Texturec> diffuseTexture, const Colord& specular)
      : PhongMaterial(diffuseTexture, specular)
    {
      setReflectionCoefficient(0.75);
      setReflectionColor(Colord::white());
    }
    
    /**
      * @returns the reflection color.
      */
    inline const Colord& reflectionColor() const {
      return m_reflectiveBRDF.reflectionColor();
    }
    
    /**
      * Sets the material's reflection color.
      * 
      * <table><tr>
      * <td>@image html reflective_material_reflection_color_red.png</td>
      * <td>@image html reflective_material_reflection_color_orange.png</td>
      * <td>@image html reflective_material_reflection_color_yellow.png</td>
      * <td>@image html reflective_material_reflection_color_green.png</td>
      * <td>@image html reflective_material_reflection_color_blue.png</td>
      * <td>@image html reflective_material_reflection_color_indigo.png</td>
      * <td>@image html reflective_material_reflection_color_violet.png</td>
      * </tr></table>
      */
    inline void setReflectionColor(const Colord& color) {
      m_reflectiveBRDF.setReflectionColor(color);
    }
    
    /**
      * @returns the reflection coefficient.
      */
    inline double reflectionCoefficient() const {
      return m_reflectiveBRDF.reflectionCoefficient();
    }
    
    /**
      * Sets the reflection coefficient.
      * 
      * <table><tr>
      * <td>@image html reflective_material_reflection_coeff_0.0.png "reflectionCoefficient=0"</td>
      * <td>@image html reflective_material_reflection_coeff_0.25.png "reflectionCoefficient=0.25"</td>
      * <td>@image html reflective_material_reflection_coeff_0.5.png "reflectionCoefficient=0.5"</td>
      * <td>@image html reflective_material_reflection_coeff_0.75.png "reflectionCoefficient=0.75"</td>
      * <td>@image html reflective_material_reflection_coeff_1.0.png "reflectionCoefficient=1"</td>
      * </tr></table>
      */
    inline void setReflectionCoefficient(double coeff) {
      m_reflectiveBRDF.setReflectionCoefficient(coeff);
    }
    
    virtual Colord shade(const Raytracer* raytracer, const Rayd& ray, const HitPoint& hitPoint, State& state) const;
    
  protected:
    PerfectSpecular m_reflectiveBRDF;
  };
}
