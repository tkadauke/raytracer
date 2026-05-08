#include <memory>
#include "render/textures/Texture.h"
#include "core/Color.h"

namespace render {
  class TextureMapping2D;

  /**
    * Texture that alternates between two child textures using mapped 2D
    * coordinates.
    *
    * Evaluation is a two-step process: first the configured
    * TextureMapping2D turns the HitPoint into texture coordinates `(s, t)`;
    * then the checker lookup chooses the bright child texture when
    * `floor(s) + floor(t)` is even and the dark child texture when it is odd.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="texture_coordinate_mapping.js"></script>
    * @endhtmlonly
    *
    * PlanarMapping2D derives `(s, t)` from the hit point's position, while
    * UVMapping2D derives it from the hit point's stored UV coordinates.
    */
  class CheckerBoardTexture : public Texturec {
  public:
    inline explicit CheckerBoardTexture(TextureMapping2D* mapping)
      : m_mapping(mapping),
        m_brightTexture(nullptr),
        m_darkTexture(nullptr)
    {
    }
    
    inline explicit CheckerBoardTexture(TextureMapping2D* mapping, std::shared_ptr<Texturec> brightTexture, std::shared_ptr<Texturec> darkTexture)
      : m_mapping(mapping),
        m_brightTexture(brightTexture),
        m_darkTexture(darkTexture)
    {
    }
    
    inline std::shared_ptr<Texturec> brightTexture() const {
      return m_brightTexture;
    }
    
    inline void setBrightTexture(std::shared_ptr<Texturec> color) {
      m_brightTexture = color;
    }
    
    inline std::shared_ptr<Texturec> darkTexture() const {
      return m_darkTexture;
    }
    
    inline void setDarkTexture(std::shared_ptr<Texturec> color) {
      m_darkTexture = color;
    }
    
    virtual Colord evaluate(const Rayd& ray, const HitPoint& hitPoint) const;
  
  private:
    TextureMapping2D* m_mapping;
    std::shared_ptr<Texturec> m_brightTexture;
    std::shared_ptr<Texturec> m_darkTexture;
  };
}
