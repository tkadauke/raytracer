#include "render/textures/TintedTexture.h"

#include "core/math/HitPoint.h"
#include "core/math/Ray.h"

#include <stdexcept>
#include <utility>

namespace render {
  TintedTexture::TintedTexture(std::shared_ptr<Texturec> texture, Colord tint)
      : m_texture(std::move(texture)),
        m_tint(tint) {
    if (!m_texture) {
      throw std::invalid_argument("TintedTexture requires a child texture");
    }
  }

  const std::shared_ptr<Texturec>& TintedTexture::texture() const {
    return m_texture;
  }

  const Colord& TintedTexture::tint() const {
    return m_tint;
  }

  Colord TintedTexture::evaluate(const Rayd& ray, const HitPoint& hitPoint) const {
    return m_texture->evaluate(ray, hitPoint) * m_tint;
  }
}
