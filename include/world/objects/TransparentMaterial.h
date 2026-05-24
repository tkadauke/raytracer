#pragma once
#include <memory>

#include "world/objects/PhongMaterial.h"

/**
  * Transparent materials are used to describe perfectly transparent materials
  * like air, water, glass, or diamonds, that may be tinted on the outside,
  * but don't filter the light while it travels through them. The [refraction
  * index](https://en.wikipedia.org/wiki/Refractive_index) describes how light
  * propagates through the medium.
  *
  * <table><tr>
  * <td>@image html transparent_material__raytracer.png "Raytracer"</td>
  * <td>@image html transparent_material__raster.png "Rasterizer"</td>
  * </tr></table>
  *
  * The software rasterizer preview is explicitly non-recursive: it shows this
  * material's local Phong base, exposes transmission as transient source alpha
  * (`1 - transmissionCoefficient`), and reports that recursive refraction and
  * reflection are available only in the raytracer.
  */
class TransparentMaterial : public PhongMaterial {
  Q_OBJECT
  Q_PROPERTY(
    double transmissionCoefficient READ transmissionCoefficient WRITE setTransmissionCoefficient)
  Q_PROPERTY(double refractionIndex READ refractionIndex WRITE setRefractionIndex)
  Q_PROPERTY(Colord reflectionColor READ reflectionColor WRITE setReflectionColor)
  Q_PROPERTY(double reflectionCoefficient READ reflectionCoefficient WRITE setReflectionCoefficient)

public:
  /**
    * Constructs a transparent material with no diffuse texture and a
    * refraction index of 1.
    */
  explicit TransparentMaterial(Element* parent = nullptr);

  /**
    * @returns the material's transmission coefficient.
    */
  inline double transmissionCoefficient() const {
    return m_transmissionCoefficient;
  }

  /**
    * Sets the material's transmission coefficient.
    *
    * <table><tr>
    * <td>@image html transparent_material_transcoeff_0.0.png "transmissionCoefficient=0"</td>
    * <td>@image html transparent_material_transcoeff_0.25.png "transmissionCoefficient=0.25"</td>
    * <td>@image html transparent_material_transcoeff_0.5.png "transmissionCoefficient=0.5"</td>
    * <td>@image html transparent_material_transcoeff_0.75.png "transmissionCoefficient=0.75"</td>
    * <td>@image html transparent_material_transcoeff_1.0.png "transmissionCoefficient=1"</td>
    * </tr></table>
    */
  inline void setTransmissionCoefficient(double coeff) {
    m_transmissionCoefficient = Ranged(0, 1).clamp(coeff);
  }

  /**
    * @returns the material's index of refraction.
    */
  inline double refractionIndex() const {
    return m_refractionIndex;
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
    m_refractionIndex = index;
  }

  /**
    * @returns the material's reflection color.
    */
  inline const Colord& reflectionColor() const {
    return m_reflectionColor;
  }

  /**
    * Sets the material's reflection color.
    *
    * <table><tr>
    * <td>@image html transparent_material_refl_color_red.png "red"</td>
    * <td>@image html transparent_material_refl_color_orange.png "orange"</td>
    * <td>@image html transparent_material_refl_color_yellow.png "yellow"</td>
    * <td>@image html transparent_material_refl_color_green.png "green"</td>
    * <td>@image html transparent_material_refl_color_blue.png "blue"</td>
    * <td>@image html transparent_material_refl_color_indigo.png "indigo"</td>
    * <td>@image html transparent_material_refl_color_violet.png "violet"</td>
    * </tr></table>
    */
  inline void setReflectionColor(const Colord& color) {
    m_reflectionColor = color;
  }

  /**
    * @returns the material's reflection coefficient.
    */
  inline double reflectionCoefficient() const {
    return m_reflectionCoefficient;
  }

  /**
    * Sets the material's reflection coefficient.
    *
    * <table><tr>
    * <td>@image html transparent_material_reflcoeff_0.0.png "reflectionCoefficient=0"</td>
    * <td>@image html transparent_material_reflcoeff_0.25.png "reflectionCoefficient=0.25"</td>
    * <td>@image html transparent_material_reflcoeff_0.5.png "reflectionCoefficient=0.5"</td>
    * <td>@image html transparent_material_reflcoeff_0.75.png "reflectionCoefficient=0.75"</td>
    * <td>@image html transparent_material_reflcoeff_1.0.png "reflectionCoefficient=1"</td>
    * </tr></table>
    */
  inline void setReflectionCoefficient(double coeff) {
    m_reflectionCoefficient = Ranged(0, 1).clamp(coeff);
  }

protected:
  virtual std::shared_ptr<render::Material> toRaytracerMaterial() const;

private:
  double m_transmissionCoefficient;
  double m_refractionIndex;
  Colord m_reflectionColor;
  double m_reflectionCoefficient;
};
