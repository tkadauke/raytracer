#pragma once

#include "render/textures/mappings/TextureMapping2D.h"

namespace render {
  class PlanarMapping2D : public TextureMapping2D {
  public:
    virtual void map(const HitPoint& hitPoint, double& s, double& t) const;
  };
}
