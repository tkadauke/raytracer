#pragma once

#include "render/textures/Texture.h"
#include "core/Color.h"

namespace render {
  class ConstantColorTexture : public Texturec {
  public:
    inline ConstantColorTexture() {
    }

    inline explicit ConstantColorTexture(const Colord& color)
        : m_color(color) {
    }

    inline const Colord& color() const {
      return m_color;
    }

    inline void setColor(const Colord& color) {
      m_color = color;
    }

    const char* typeName() const noexcept override {
      return "ConstantColorTexture";
    }

    Colord evaluate(const Rayd& ray, const HitPoint& hitPoint) const override;

  private:
    Colord m_color;
  };
}
