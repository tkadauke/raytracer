#pragma once
#include <memory>

#include "world/objects/Element.h"
#include "render/textures/Texture.h"
#include "core/Color.h"

/**
  * Represents a texture.
  */
class Texture : public Element {
  Q_OBJECT;
  
public:
  /**
    * @returns the default texture, which is a black constant color texture.
    */
  static Texture* defaultTexture();
  
  /**
    * Default constructor.
    */
  explicit Texture(Element* parent = nullptr);

  virtual std::shared_ptr<render::Texturec> toRaytracerTexture() const = 0;
};

/**
  * @returns the result of Texture::defaultTexture() if texture is a nullptr,
  *   or texture otherwise.
  */
inline Texture* textureOrDefault(Texture* texture) {
  return texture ? texture : Texture::defaultTexture();
}
