#pragma once

#include "world/objects/PhongMaterial.h"

/**
  * Reflective materials describe shiny objects like polished metal or
  * mirrors.
  *
  * @image html reflective_material_red.png "Reflective material"
  */
class ReflectiveMaterial : public PhongMaterial {
  Q_OBJECT;
  Q_PROPERTY(Colord reflectionColor READ reflectionColor WRITE setReflectionColor);
  Q_PROPERTY(double reflectionCoefficient READ reflectionCoefficient WRITE setReflectionCoefficient);

public:
  /**
    * Constructs a default reflective material with no diffuse texture, a
    * reflection coefficient of 0.75 and a white reflection color.
    */
  explicit ReflectiveMaterial(Element* parent = nullptr);

  /**
    * @returns the reflection color.
    */
  inline const Colord& reflectionColor() const {
    return m_reflectionColor;
  }

  /**
    * Sets the material's reflection color.
    *
    * <table><tr>
    * <td>@image html reflective_material_reflection_color_red.png "red"</td>
    * <td>@image html reflective_material_reflection_color_orange.png "orange"</td>
    * <td>@image html reflective_material_reflection_color_yellow.png "yellow"</td>
    * <td>@image html reflective_material_reflection_color_green.png "green"</td>
    * <td>@image html reflective_material_reflection_color_blue.png "blue"</td>
    * <td>@image html reflective_material_reflection_color_indigo.png "indigo"</td>
    * <td>@image html reflective_material_reflection_color_violet.png "violet"</td>
    * </tr></table>
    */
  inline void setReflectionColor(const Colord& color) {
    m_reflectionColor = color;
  }

  /**
    * @returns the reflection coefficient.
    */
  inline double reflectionCoefficient() const {
    return m_reflectionCoefficient;
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
    m_reflectionCoefficient = coeff;
  }

protected:
  virtual std::shared_ptr<raytracer::Material> toRaytracerMaterial() const;

private:
  Colord m_reflectionColor;
  double m_reflectionCoefficient;
};
