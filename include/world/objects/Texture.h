#pragma once

#include "world/objects/Element.h"
#include "raytracer/textures/Texture.h"
#include "core/Color.h"

class Texture : public Element {
  Q_OBJECT
  
public:
  static Texture* defaultTexture();
  
  Texture(Element* parent = nullptr);

  virtual raytracer::Texturec* toRaytracerTexture() const = 0;
};

inline Texture* textureOrDefault(Texture* texture) {
  return texture ? texture : Texture::defaultTexture();
}
