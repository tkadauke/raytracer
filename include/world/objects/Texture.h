#pragma once

#include "world/objects/Element.h"
#include "core/Color.h"

namespace raytracer {
  template<class T>
  class Texture;
}

class Texture : public Element {
  Q_OBJECT
  
public:
  static Texture* defaultTexture();
  
  Texture(Element* parent = nullptr);

  virtual raytracer::Texture<Colord>* toRaytracerTexture() const = 0;
};

inline Texture* textureOrDefault(Texture* texture) {
  return texture ? texture : Texture::defaultTexture();
}
