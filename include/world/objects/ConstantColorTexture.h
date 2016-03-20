#pragma once

#include "world/objects/Texture.h"
#include "core/Color.h"

/**
  * Represents a texture that has the same color regardless of position.
  */
class ConstantColorTexture : public Texture {
  Q_OBJECT;
  Q_PROPERTY(Colord color READ color WRITE setColor);
  
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
    * <td>@image html constant_color_red.png</td>
    * <td>@image html constant_color_orange.png</td>
    * <td>@image html constant_color_yellow.png</td>
    * <td>@image html constant_color_green.png</td>
    * <td>@image html constant_color_blue.png</td>
    * <td>@image html constant_color_indigo.png</td>
    * <td>@image html constant_color_violet.png</td>
    * </tr></table>
    */
  inline void setColor(const Colord& color) {
    m_color = color;
  }
  
  virtual std::shared_ptr<raytracer::Texturec> toRaytracerTexture() const;

private:
  Colord m_color;
};
