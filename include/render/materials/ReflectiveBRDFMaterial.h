#pragma once
#include "render/materials/PhongMaterial.h"
#include "render/brdf/PerfectSpecular.h"

namespace render {
  /**
    * Base for materials that mix a perfect-specular mirror-reflection BRDF
    * term into PhongMaterial's local shading, exposing its reflection color
    * and coefficient. Shared by ReflectiveMaterial and TransparentMaterial,
    * which each sample this BRDF for their mirror-reflection branch.
    */
  class ReflectiveBRDFMaterial : public PhongMaterial {
  public:
    using PhongMaterial::PhongMaterial;

    /**
      * @returns the reflection color.
      */
    inline const Colord& reflectionColor() const {
      return m_reflectiveBRDF.reflectionColor();
    }

    /**
      * Sets the material's reflection color.
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
      */
    inline void setReflectionCoefficient(double coeff) {
      m_reflectiveBRDF.setReflectionCoefficient(coeff);
    }

  protected:
    render::PerfectSpecular m_reflectiveBRDF;
  };
}
