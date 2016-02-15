#pragma once

#include "raytracer/materials/PhongMaterial.h"

namespace raytracer {
  class TransparentMaterial : public PhongMaterial {
  public:
    inline TransparentMaterial()
      : PhongMaterial(), m_refractionIndex(1)
    {
    }

    inline TransparentMaterial(const Colord& color)
      : PhongMaterial(color), m_refractionIndex(1)
    {
    }

    inline void setAbsorbanceColor(const Colord& color) { m_absorbanceColor = color; }
    inline const Colord& absorbanceColor() const { return m_absorbanceColor; }

    inline void setRefractionIndex(double index) { m_refractionIndex = index; }
    inline double refractionIndex() const { return m_refractionIndex; }

    virtual Colord shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth);

  private:
    Vector3d refract(const Vector3d& direction, const Vector3d& normal, double outerRefractionIndex, double innerRefractionIndex);
    bool totalInternalReflection(const Ray& ray, const HitPoint& hitPoint);

    Colord m_absorbanceColor;
    double m_refractionIndex;
  };
}
