#pragma once
#include <memory>

#include "render/materials/PhongMaterial.h"
#include "render/brdf/PerfectSpecular.h"

namespace render {
  /**
    * Reflective materials describe shiny objects like polished metal or
    * mirrors.
    * 
    * Same reflective sphere through both engines — the comparison
    * makes the reflection contribution visible by its absence:
    *
    * <table>
    *   <tr>
    *     <th>Raytracer</th>
    *     <th>Software rasterizer</th>
    *   </tr>
    *   <tr>
    *     <td>@image html reflective_material_red__raytracer.png ""</td>
    *     <td>@image html reflective_material_red__raster.png ""</td>
    *   </tr>
    * </table>
    *
    * The raytracer fires a recursive ray off the surface in the
    * mirror direction and uses the resulting color as the visible
    * appearance — the sphere shows the floor and sky reflected back.
    * The rasterizer doesn't recurse, so its documented fallback displays
    * only the local Phong base and `rendercli --engine raster` reports
    * that mirror recursion was dropped. If there is no usable diffuse
    * texture, that base falls back to a per-face hash color just to keep
    * the silhouette readable. The comparison is "this is what the
    * recursion buys you."
    *
    * The mirror direction is a deterministic BRDF sample: if `d` is
    * the incoming ray direction at the hit point and `n` is the
    * surface normal, the reflected direction is `d - 2(d dot n)n`.
    * The material then asks the raytracer to shade that new ray, so
    * reflections can see other reflective objects, and so on until
    * the recursion limit stops the tree. The reflection coefficient
    * scales the contribution from each reflected branch.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="reflective_material_recursion.js"></script>
    * @endhtmlonly
    */
  class ReflectiveMaterial : public PhongMaterial {
  public:
    /**
      * Constructs a default reflective material with no diffuse texture, a
      * reflection coefficient of 0.75 and a white reflection color.
      */
    inline ReflectiveMaterial()
        : PhongMaterial() {
      setReflectionCoefficient(0.75);
      setReflectionColor(Colord::white());
    }

    /**
      * Constructs a reflective material with diffuseTexture, a reflection
      * coefficient of 0.75 and a white reflection color.
      */
    inline explicit ReflectiveMaterial(std::shared_ptr<render::Texturec> diffuseTexture)
        : PhongMaterial(diffuseTexture) {
      setReflectionCoefficient(0.75);
      setReflectionColor(Colord::white());
    }

    /**
      * Constructs a reflective material with diffuseTexture, the given specular
      * color, a reflection coefficient of 0.75 and a white reflection color.
      */
    inline explicit ReflectiveMaterial(std::shared_ptr<render::Texturec> diffuseTexture,
                                       const Colord& specular)
        : PhongMaterial(diffuseTexture, specular) {
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
      * <td>@image html reflective_material_reflection_color_red.png "red"</td>
      * <td>@image html reflective_material_reflection_color_yellow.png "yellow"</td>
      * <td>@image html reflective_material_reflection_color_orange.png "orange"</td>
      * <td>@image html reflective_material_reflection_color_green.png "green"</td>
      * <td>@image html reflective_material_reflection_color_blue.png "blue"</td>
      * <td>@image html reflective_material_reflection_color_indigo.png "indigo"</td>
      * <td>@image html reflective_material_reflection_color_violet.png "violet"</td>
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

    Colord shade(const render::RayCaster* raycaster, const render::Scene& scene, const Rayd& ray,
                 const HitPoint& hitPoint, render::State& state) const override;

    WhittedShadeResult shadeWhitted(const render::RayCaster* raycaster, const render::Scene& scene,
                                    const Rayd& ray, const HitPoint& hitPoint,
                                    render::State& state) const override;

    bool requiresWhittedPacketHitRefinement() const override {
      return false;
    }

    const char* whittedPacketHitRefinementLabel() const override {
      return "reflective";
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
      return RasterRecursiveFallback::ReflectiveLocalPhong;
    }

    const char* rasterRecursiveFallbackWarning() const override {
      return "Rasterizer fallback: ReflectiveMaterial previews only its local Phong base; "
             "mirror recursion remains raytracer-only.";
    }

  protected:
    render::PerfectSpecular m_reflectiveBRDF;
  };
}
