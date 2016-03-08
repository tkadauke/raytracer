#include "raytracer/textures/Texture.h"
#include "core/Color.h"

namespace raytracer {
  class ConstantColorTexture : public Texturec {
  public:
    inline ConstantColorTexture() {}
    
    inline ConstantColorTexture(const Colord& color)
      : m_color(color)
    {
    }
    
    inline void setColor(const Colord& color) {
      m_color = color;
    }
    
    inline const Colord& color() const {
      return m_color;
    }
    
    virtual Colord evaluate(const Rayd& ray, const HitPoint& hitPoint) const;
  
  private:
    Colord m_color;
  };
}
