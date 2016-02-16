#pragma once

#include "raytracer/materials/PhongMaterial.h"

namespace raytracer {
  class ReflectiveMaterial : public PhongMaterial {
  public:
    inline ReflectiveMaterial()
      : PhongMaterial()
    {
    }

    inline ReflectiveMaterial(const Colord& color)
      : PhongMaterial(color)
    {
    }

    inline ReflectiveMaterial(const Colord& diffuse, const Colord& specular)
      : PhongMaterial(diffuse, specular)
    {
    }

    virtual Colord shade(Raytracer* raytracer, const Ray& ray, const HitPoint& hitPoint, int recursionDepth);
  };
}
