#pragma once

#include "render/textures/Texture.h"

namespace render {
  class UVColorTexture : public Texturec {
  public:
    /**
      * Maps texture coordinates to the diagnostic color used by this texture.
      * Kept here so CPU, raster, and GPU-reference evaluation agree on the
      * exact mapping without duplicating it.
      */
    [[nodiscard]] static Colord colorAt(const Vector2d& uv);

    const char* typeName() const noexcept override {
      return "UVColorTexture";
    }

    Colord evaluate(const Rayd& ray, const HitPoint& hitPoint) const override;
  };
}
