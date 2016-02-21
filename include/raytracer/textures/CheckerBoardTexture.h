#include "raytracer/textures/Texture.h"
#include "core/Color.h"

namespace raytracer {
  class TextureMapping2D;
  
  class CheckerBoardTexture : public Texturec {
  public:
    inline CheckerBoardTexture(TextureMapping2D* mapping)
      : m_mapping(mapping), m_brightTexture(nullptr), m_darkTexture(nullptr)
    {
    }
    
    inline CheckerBoardTexture(TextureMapping2D* mapping, Texturec* brightTexture, Texturec* darkTexture)
      : m_mapping(mapping), m_brightTexture(brightTexture), m_darkTexture(darkTexture)
    {
    }
    
    inline void setBrightTexture(Texturec* color) { m_brightTexture = color; }
    inline Texturec* brightTexture() const { return m_brightTexture; }
    
    inline void setDarkTexture(Texturec* color) { m_darkTexture = color; }
    inline Texturec* darkTexture() const { return m_darkTexture; }
    
    virtual Colord evaluate(const Ray& ray, const HitPoint& hitPoint) const;
  
  private:
    TextureMapping2D* m_mapping;
    Texturec* m_brightTexture;
    Texturec* m_darkTexture;
  };
}
