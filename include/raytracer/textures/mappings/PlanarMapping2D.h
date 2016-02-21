#pragma once

#include "raytracer/textures/mappings/TextureMapping2D.h"

namespace raytracer {
  class PlanarMapping2D : public TextureMapping2D {
  public:
    virtual void map(const HitPoint& hitPoint, double& s, double& t) const;
  };
}
