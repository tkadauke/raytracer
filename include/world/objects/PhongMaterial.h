#pragma once

#include "world/objects/MatteMaterial.h"
#include "core/math/Range.h"
#include "core/Color.h"

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
  Q_OBJECT;
  Q_PROPERTY(Colord specularColor READ specularColor WRITE setSpecularColor);
  Q_PROPERTY(double exponent READ exponent WRITE setExponent);
  Q_PROPERTY(double specularCoefficient READ specularCoefficient WRITE setSpecularCoefficient);

public:
  /**
    * Constructs a default Phong material with no diffuse texture and a white
    * specular color.
    */
  explicit PhongMaterial(Element* parent = nullptr);

  /**
    * @returns the material's specular color.
    */
  inline const Colord& specularColor() const {
    return m_specularColor;
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
    m_specularColor = color;
  }

  /**
    * @returns the material's specular reflection coefficient.
    */
  inline double specularCoefficient() const {
    return m_specularCoefficient;
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
    m_specularCoefficient = Ranged(0, 1).clamp(coeff);
  }

  /**
    * @returns the material's lobe exponent.
    */
  inline double exponent() const {
    return m_exponent;
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
    m_exponent = exponent;
  }

protected:
  virtual std::shared_ptr<raytracer::Material> toRaytracerMaterial() const;

private:
  Colord m_specularColor;
  double m_exponent;
  double m_specularCoefficient;
};
