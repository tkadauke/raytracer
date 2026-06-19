#pragma once

#include "render/textures/Texture.h"

namespace render {
  class UVColorTexture : public Texturec {
  public:
    const char* typeName() const noexcept override {
      return "UVColorTexture";
    }

    Colord evaluate(const Rayd& ray, const HitPoint& hitPoint) const override;
  };
}
