#pragma once
#include <memory>

#include "render/materials/PhongMaterial.h"
#include "render/brdf/PerfectSpecular.h"
#include "render/brdf/PerfectTransmitter.h"

namespace render {
  /**
    * Transparent materials are used to describe perfectly transparent materials
    * like air, water, glass, or diamonds, that may be tinted on the outside,
    * but don't filter the light while it travels through them. The [refraction
    * index](https://en.wikipedia.org/wiki/Refractive_index) describes how light
    * propagates through the medium.
    *
    * Same transparent sphere through both engines:
    *
    * <table>
    *   <tr>
    *     <th>Raytracer</th>
    *     <th>Software rasterizer</th>
    *   </tr>
    *   <tr>
    *     <td>@image html transparent_material__raytracer.png ""</td>
    *     <td>@image html transparent_material__raster.png ""</td>
    *   </tr>
    * </table>
    *
    * The raytracer fires a refracted ray through the surface (and
    * a reflected ray for the Fresnel-style reflection on the silhouette)
    * and uses the resulting colours as the visible appearance — the
    * sphere shows the floor and sky bent through it. The rasterizer
    * doesn't recurse, so it has nothing to display other than the
    * diffuse base; here that base is the medium-grey configured by
    * the doc-render driver, giving an opaque grey sphere instead.
    * The comparison is "this is what the recursion buys you" — same
    * lesson as for the reflective material.
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
      setTransmissionCoefficient(1);
    }

    /**
      * Constructs a transparent material with diffuseTexture and a refraction
      * index of 1.
      */
    inline explicit TransparentMaterial(std::shared_ptr<render::Texturec> diffuseTexture)
      : PhongMaterial(diffuseTexture)
    {
      setRefractionIndex(1);
    }

    /**
      * @returns the material's reflection color.
      */
    inline const Colord& reflectionColor() const {
      return m_reflectiveBRDF.reflectionColor();
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
      m_reflectiveBRDF.setReflectionColor(color);
    }

    /**
      * @returns the material's reflection coefficient.
      */
    inline double reflectionCoefficient() const {
      return m_reflectiveBRDF.reflectionCoefficient();
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
      m_reflectiveBRDF.setReflectionCoefficient(coeff);
    }

    /**
      * @returns the material's transmission coefficient.
      */
    inline double transmissionCoefficient() const {
      return m_specularBTDF.transmissionCoefficient();
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
      m_specularBTDF.setTransmissionCoefficient(coeff);
    }

    /**
      * @returns the material's index of refraction.
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

    virtual Colord shade(const render::RayCaster* raycaster, const render::Scene& scene, const Rayd& ray, const HitPoint& hitPoint, render::State& state) const;

  private:
    Vector3d refract(const Vector3d& direction, const Vector3d& normal, double outerRefractionIndex, double innerRefractionIndex);
    bool totalInternalReflection(const Rayd& ray, const HitPoint& hitPoint);

    render::PerfectSpecular m_reflectiveBRDF;
    render::PerfectTransmitter m_specularBTDF;
  };
}
