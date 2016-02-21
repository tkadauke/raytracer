#pragma once

#include "world/objects/Texture.h"
#include "core/Color.h"

class ConstantColorTexture : public Texture {
  Q_OBJECT
  Q_PROPERTY(Colord color READ color WRITE setColor);
  
public:
  ConstantColorTexture(Element* parent = nullptr);

  inline void setColor(const Colord& color) { m_color = color; }
  inline const Colord& color() const { return m_color; }

  virtual std::shared_ptr<raytracer::Texturec> toRaytracerTexture() const;

private:
  Colord m_color;
};
