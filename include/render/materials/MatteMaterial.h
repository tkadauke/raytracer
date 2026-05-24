#pragma once
#include <memory>

#include "render/materials/Material.h"
#include "render/brdf/Lambertian.h"
#include "render/textures/Texture.h"

namespace render {
  /**
    * Matte materials have no reflection, or transmission. As the name suggests,
    * they appear matte.
    * 
    * Same red-matte sphere through both engines. The matte material
    * is the simplest case: pure Lambertian with no recursion, so
    * both engines produce the same fundamental shading model.
    * The raytracer additionally renders the textured floor
    * reflection (because the floor itself is reflective) and a soft
    * shadow under the sphere; the rasterizer produces the same red
    * shaded sphere on its default background.
    *
    * MatteMaterial's diffuse component is Lambertian: the brightness
    * from each light follows `max(n dot l, 0)` and does not depend on
    * the camera/view direction.
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
    *     <td>@image html matte_material_red__raytracer.png ""</td>
    *     <td>@image html matte_material_red__raster.png ""</td>
    *   </tr>
    * </table>
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
          m_normalTexture(nullptr),
          m_ambientCoefficient(1),
          m_diffuseCoefficient(1) {
    }

    /**
      * Constructs the default matte material with the given texture and ambient
      * and diffuse coefficients of 1.
      */
    inline explicit MatteMaterial(std::shared_ptr<render::Texturec> texture)
        : Material(),
          m_diffuseTexture(texture),
          m_normalTexture(nullptr),
          m_ambientCoefficient(1),
          m_diffuseCoefficient(1) {
    }

    /**
      * @returns the diffuse texture.
      */
    inline std::shared_ptr<render::Texturec> diffuseTexture() const {
      return m_diffuseTexture;
    }

    /**
      * Sets the material's diffuse texture.
      * 
      * <table><tr>
      * <td>@image html matte_material_rainbow_red.png "red"</td>
      * <td>@image html matte_material_rainbow_orange.png "orange"</td>
      * <td>@image html matte_material_rainbow_yellow.png "yellow"</td>
      * <td>@image html matte_material_rainbow_green.png "green"</td>
      * <td>@image html matte_material_rainbow_blue.png "blue"</td>
      * <td>@image html matte_material_rainbow_indigo.png "indigo"</td>
      * <td>@image html matte_material_rainbow_violet.png "violet"</td>
      * </tr></table>
      */
    inline void setDiffuseTexture(std::shared_ptr<render::Texturec> texture) {
      m_diffuseTexture = texture;
    }

    /**
      * @returns the tangent-space normal map texture.
      */
    inline std::shared_ptr<render::Texturec> normalTexture() const {
      return m_normalTexture;
    }

    /**
      * Sets the tangent-space normal map texture. Raster material evaluation
      * decodes RGB as `(x, y, z) = color * 2 - 1` and transforms the result
      * through the triangle's UV-derived tangent frame. When a triangle has no
      * usable UV tangent frame, raster shading falls back to the interpolated
      * geometric normal.
      *
      * <table><tr>
      * <td>@image html rasterizer_normal_map_flat.png "flat normals"</td>
      * <td>@image html rasterizer_normal_map_mapped.png "normal mapped"</td>
      * </tr></table>
      */
    inline void setNormalTexture(std::shared_ptr<render::Texturec> texture) {
      m_normalTexture = texture;
    }

    /**
      * @returns the ambient light coefficient.
      */
    inline double ambientCoefficient() const {
      return m_ambientCoefficient;
    }

    /**
      * Sets the ambient light coefficient.
      * 
      * <table><tr>
      * <td>@image html matte_material_ambient_0.0.png "ambientCoefficient=0"</td>
      * <td>@image html matte_material_ambient_0.25.png "ambientCoefficient=0.25"</td>
      * <td>@image html matte_material_ambient_0.5.png "ambientCoefficient=0.5"</td>
      * <td>@image html matte_material_ambient_0.75.png "ambientCoefficient=0.75"</td>
      * <td>@image html matte_material_ambient_1.0.png "ambientCoefficient=1"</td>
      * </tr></table>
      */
    inline void setAmbientCoefficient(double coeff) {
      m_ambientCoefficient = coeff;
    }

    /**
      * @returns the diffuse light coefficient.
      */
    inline double diffuseCoefficient() const {
      return m_diffuseCoefficient;
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

    virtual Colord shade(const render::RayCaster* raycaster, const render::Scene& scene,
                         const Rayd& ray, const HitPoint& hitPoint, render::State& state) const;

  private:
    std::shared_ptr<render::Texturec> m_diffuseTexture;
    std::shared_ptr<render::Texturec> m_normalTexture;
    double m_ambientCoefficient;
    double m_diffuseCoefficient;
  };
}
