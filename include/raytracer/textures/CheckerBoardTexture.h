#include "raytracer/textures/Texture.h"
#include "core/Color.h"

namespace raytracer {
  class TextureMapping2D;
  
  class CheckerBoardTexture : public Texture<Colord> {
  public:
    inline CheckerBoardTexture(TextureMapping2D* mapping)
      : m_mapping(mapping), m_brightTexture(nullptr), m_darkTexture(nullptr)
    {
    }
    
    inline CheckerBoardTexture(TextureMapping2D* mapping, Texture<Colord>* brightTexture, Texture<Colord>* darkTexture)
      : m_mapping(mapping), m_brightTexture(brightTexture), m_darkTexture(darkTexture)
    {
    }
    
    inline void setBrightTexture(Texture<Colord>* color) { m_brightTexture = color; }
    inline Texture<Colord>* brightTexture() const { return m_brightTexture; }
    
    inline void setDarkTexture(Texture<Colord>* color) { m_darkTexture = color; }
    inline Texture<Colord>* darkTexture() const { return m_darkTexture; }
    
    virtual Colord evaluate(const Ray& ray, const HitPoint& hitPoint) const;
  
  private:
    TextureMapping2D* m_mapping;
    Texture<Colord>* m_brightTexture;
    Texture<Colord>* m_darkTexture;
  };
}
