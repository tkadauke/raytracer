#pragma once

#include "world/objects/Texture.h"
#include "core/Color.h"

/**
  * Represents a checker box texture.
  * 
  * @image html checker_board.png "Checker board texture with white and black fields"
  */
class CheckerBoardTexture : public Texture {
  Q_OBJECT;
  Q_PROPERTY(Texture* brightTexture READ brightTexture WRITE setBrightTexture);
  Q_PROPERTY(Texture* darkTexture READ darkTexture WRITE setDarkTexture);
  
public:
  /**
    * Default constructor. Creates a checker board texture with no brigth and
    * dark textures.
    */
  CheckerBoardTexture(Element* parent = nullptr);

  /**
    * @returns the checker board's bright texture.
    */
  inline Texture* brightTexture() const {
    return m_brightTexture;
  }
  
  /**
    * Sets the checker board's texture for the bright fields.
    * 
    * <table><tr>
    * <td>@image html checker_board_bright_color_red.png</td>
    * <td>@image html checker_board_bright_color_orange.png</td>
    * <td>@image html checker_board_bright_color_yellow.png</td>
    * <td>@image html checker_board_bright_color_green.png</td>
    * <td>@image html checker_board_bright_color_blue.png</td>
    * <td>@image html checker_board_bright_color_indigo.png</td>
    * <td>@image html checker_board_bright_color_violet.png</td>
    * </tr></table>
    */
  inline void setBrightTexture(Texture* texture) {
    if (texture == this) {
      m_brightTexture = nullptr;
    } else {
      m_brightTexture = texture;
    }
  }
  
  /**
    * @returns the checker board's dark texture.
    */
  inline Texture* darkTexture() const {
    return m_darkTexture;
  }

  /**
    * Sets the checker board's texture for the bright fields.
    * 
    * <table><tr>
    * <td>@image html checker_board_dark_color_red.png</td>
    * <td>@image html checker_board_dark_color_orange.png</td>
    * <td>@image html checker_board_dark_color_yellow.png</td>
    * <td>@image html checker_board_dark_color_green.png</td>
    * <td>@image html checker_board_dark_color_blue.png</td>
    * <td>@image html checker_board_dark_color_indigo.png</td>
    * <td>@image html checker_board_dark_color_violet.png</td>
    * </tr></table>
    */
  inline void setDarkTexture(Texture* texture) {
    if (texture == this) {
      m_darkTexture = nullptr;
    } else {
      m_darkTexture = texture;
    }
  }
  
  virtual std::shared_ptr<raytracer::Texturec> toRaytracerTexture() const;

private:
  Colord m_color;
  Texture* m_brightTexture;
  Texture* m_darkTexture;
};
