#pragma once

#include "raytracer/materials/Material.h"

namespace raytracer {
  class PhongMaterial : public Material {
  public:
    inline PhongMaterial()
      : m_highlightColor(1, 1, 1)
    {
    }

    inline PhongMaterial(const Colord& color)
      : m_diffuseColor(color), m_highlightColor(1, 1, 1)
    {
    }

    inline PhongMaterial(const Colord& diffuse, const Colord& highlight)
      : m_diffuseColor(diffuse), m_highlightColor(highlight)
    {
    }

    inline PhongMaterial(const Colord& diffuse, const Colord& highlight, double exponent)
      : m_diffuseColor(diffuse), m_highlightColor(highlight), m_exponent(exponent)
    {
    }

    inline void setDiffuseColor(const Colord& color) { m_diffuseColor = color; }
    inline const Colord& diffuseColor() const { return m_diffuseColor; }

    inline void setHighlightColor(const Colord& color) { m_highlightColor = color; }
    inline const Colord& highlightColor() const { return m_highlightColor; }

    inline void setExponent(double exponent) { m_exponent = exponent; }
    inline double exponent() const { return m_exponent; }

    virtual Colord shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth);

  private:
    Colord m_diffuseColor, m_highlightColor;
    double m_exponent;
  };
}
