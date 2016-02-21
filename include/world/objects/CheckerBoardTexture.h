#pragma once

#include "world/objects/Texture.h"
#include "core/Color.h"

class CheckerBoardTexture : public Texture {
  Q_OBJECT
  Q_PROPERTY(Texture* brightTexture READ brightTexture WRITE setBrightTexture);
  Q_PROPERTY(Texture* darkTexture READ darkTexture WRITE setDarkTexture);
  
public:
  CheckerBoardTexture(Element* parent = nullptr);

  inline void setBrightTexture(Texture* texture) {
    if (texture == this) {
      m_brightTexture = nullptr;
    } else {
      m_brightTexture = texture;
    }
  }
  inline Texture* brightTexture() const { return m_brightTexture; }
  
  inline void setDarkTexture(Texture* texture) {
    if (texture == this) {
      m_darkTexture = nullptr;
    } else {
      m_darkTexture = texture;
    }
  }
  inline Texture* darkTexture() const { return m_darkTexture; }

  virtual std::shared_ptr<raytracer::Texturec> toRaytracerTexture() const;

private:
  Colord m_color;
  Texture* m_brightTexture;
  Texture* m_darkTexture;
};
