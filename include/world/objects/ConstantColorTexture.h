#pragma once
#include <memory>

#include "world/objects/Texture.h"
#include "core/Color.h"

/**
  * Represents a texture that has the same color regardless of position.
  */
class ConstantColorTexture : public Texture {
  Q_OBJECT
  Q_PROPERTY(Colord color READ color WRITE setColor)
  
public:
  /**
    * Default constructor. Creates a black texture.
    */
  explicit ConstantColorTexture(Element* parent = nullptr);

  /**
    * @returns the texture's color.
    */
  inline const Colord& color() const {
    return m_color;
  }
  
  /**
    * Sets the texture's color.
    * 
    * <table><tr>
    * <td>@image html constant_color_red.png "red"</td>
    * <td>@image html constant_color_orange.png "orange"</td>
    * <td>@image html constant_color_yellow.png "yellow"</td>
    * <td>@image html constant_color_green.png "green"</td>
    * <td>@image html constant_color_blue.png "blue"</td>
    * <td>@image html constant_color_indigo.png "indigo"</td>
    * <td>@image html constant_color_violet.png "violet"</td>
    * </tr></table>
    */
  inline void setColor(const Colord& color) {
    m_color = color;
  }
  
  virtual std::shared_ptr<render::Texturec> toRaytracerTexture() const;

private:
  Colord m_color;
};
