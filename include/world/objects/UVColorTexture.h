#pragma once
#include <memory>

#include "world/objects/Texture.h"

/**
  * Diagnostic texture that visualises UV coordinates as colour:
  * red = u, green = v, blue = 0.
  */
class UVColorTexture : public Texture {
public:
  explicit UVColorTexture(Element* parent = nullptr);

  std::shared_ptr<render::Texturec> toRaytracerTexture() const override;
};
