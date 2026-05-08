#pragma once

#include "render/textures/Texture.h"

namespace render {
  class UVColorTexture : public Texturec {
  public:
    Colord evaluate(const Rayd& ray, const HitPoint& hitPoint) const override;
  };
}
