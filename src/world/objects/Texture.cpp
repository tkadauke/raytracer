#include "world/objects/Texture.h"
#include "world/objects/ConstantColorTexture.h"

Texture* Texture::defaultTexture() {
  static ConstantColorTexture* result = nullptr;
  if (!result) {
    result = new ConstantColorTexture;
    result->setColor(Colord::black());
  }
  
  return result;
}

Texture::Texture(Element* parent)
  : Element(parent)
{
}

#include "Texture.moc"
