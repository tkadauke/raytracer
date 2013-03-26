#ifndef PHONG_MATERIAL_H
#define PHONG_MATERIAL_H

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

    inline void setDiffuseColor(const Colord& color) { m_diffuseColor = color; }
    inline const Colord& diffuseColor() const { return m_diffuseColor; }

    inline void setHighlightColor(const Colord& color) { m_highlightColor = color; }
    inline const Colord& highlightColor() const { return m_highlightColor; }

    virtual Colord shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth);

  private:
    Colord m_diffuseColor, m_highlightColor;
  };
}

#endif
