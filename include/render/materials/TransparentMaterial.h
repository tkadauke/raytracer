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
    * and uses the resulting colors as the visible appearance — the
    * sphere shows the floor and sky bent through it. The rasterizer
    * doesn't recurse, so its documented fallback displays only the local
    * Phong base and exposes transmission as transient source alpha
    * (`1 - transmissionCoefficient`) for alpha testing and source-alpha
    * blending; `rendercli --engine raster` reports that refraction and
    * reflection recursion were dropped. In the rendered comparison that
    * local base is the medium-gray configured by the doc-render driver.
    * The comparison is "this is what the recursion buys you" — same
    * lesson as for the reflective material.
    *
    * Refraction follows Snell's law: the incoming angle, the surface normal,
    * and the ratio between the current medium's IOR and the next medium's
    * IOR determine the transmitted ray direction. When a ray tries to leave a
    * denser medium at an angle greater than the critical angle, no real
    * transmitted direction exists, so the material traces only the mirror
    * reflection branch. That total internal reflection is why high-IOR glass
    * or diamond can trap rays until they strike the surface closer to the
    * normal.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="transparent_material_refraction.js"></script>
    * @endhtmlonly
    */
  class TransparentMaterial : public PhongMaterial {
  public:
    /**
      * Constructs a transparent material with no diffuse texture and a
      * refraction index of 1.
      */
    inline TransparentMaterial()
        : PhongMaterial() {
      setRefractionIndex(1);
      setTransmissionCoefficient(1);
    }

    /**
      * Constructs a transparent material with diffuseTexture and a refraction
      * index of 1.
      */
    inline explicit TransparentMaterial(std::shared_ptr<render::Texturec> diffuseTexture)
        : PhongMaterial(diffuseTexture) {
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

    Colord shade(const render::RayCaster* raycaster, const render::Scene& scene, const Rayd& ray,
                 const HitPoint& hitPoint, render::State& state) const override;

    WhittedShadeResult shadeWhitted(const render::RayCaster* raycaster, const render::Scene& scene,
                                    const Rayd& ray, const HitPoint& hitPoint,
                                    render::State& state) const override;

    bool requiresWhittedPacketHitRefinement() const override {
      return true;
    }

    const char* whittedPacketHitRefinementLabel() const override {
      return "transparent";
    }

    bool supportsBsdfSampling() const override {
      return true;
    }

    Colord evalBsdf(const HitPoint& hitPoint, const Vector3d& wi,
                    const Vector3d& wo) const override;

    MaterialBsdfSample sampleBsdf(const HitPoint& hitPoint, const Vector3d& wi,
                                  const Vector2d& sample) const override;

    double bsdfPdf(const HitPoint& hitPoint, const Vector3d& wi, const Vector3d& wo) const override;

    RasterRecursiveFallback rasterRecursiveFallback() const override {
      return RasterRecursiveFallback::TransparentAlphaPhong;
    }

    double rasterPreviewAlpha() const override {
      return 1.0 - transmissionCoefficient();
    }

    const char* rasterRecursiveFallbackWarning() const override {
      return "Rasterizer fallback: TransparentMaterial previews its local Phong base with "
             "source alpha from transmission; refraction/reflection recursion remains "
             "raytracer-only.";
    }

  private:
    struct BsdfSamplingWeights {
      double local{1.0};
      double reflection{0.0};
      double transmission{0.0};
    };

    BsdfSamplingWeights bsdfSamplingWeights(bool totalInternalReflection) const;
    MaterialBsdfSample sampleReflectionBsdf(const HitPoint& hitPoint, const Vector3d& wi,
                                            double selectionWeight) const;
    MaterialBsdfSample sampleTransmissionBsdf(const HitPoint& hitPoint, const Vector3d& wi,
                                              double selectionWeight) const;
    MaterialBsdfSample sampleTotalInternalReflectionBsdf(const HitPoint& hitPoint,
                                                         const Vector3d& wi) const;

    render::PerfectSpecular m_reflectiveBRDF;
    render::PerfectTransmitter m_specularBTDF;
  };
}
