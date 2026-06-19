#pragma once

#include "core/Color.h"
#include "render/textures/Texture.h"

#include <memory>

namespace render {
  class TintedTexture : public Texturec {
  public:
    TintedTexture(std::shared_ptr<Texturec> texture, Colord tint);

    const std::shared_ptr<Texturec>& texture() const;
    const Colord& tint() const;
    const char* typeName() const noexcept override;
    Colord evaluate(const Rayd& ray, const HitPoint& hitPoint) const override;

  private:
    std::shared_ptr<Texturec> m_texture;
    Colord m_tint;
  };
}
