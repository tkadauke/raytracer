#pragma once
#include <memory>

#include "world/objects/Texture.h"
#include "core/Color.h"
#include <QString>

/**
  * Represents a checker box texture.
  *
  * The editable wrapper exposes the runtime checker texture's mapping choice:
  * `planar` uses the hit point position, while `uv` uses stored UV coordinates
  * with `uScale` and `vScale`.
  * 
  * @image html checker_board.png "Checker board texture with white and black fields"
  *
  * @see render::CheckerBoardTexture
  */
class CheckerBoardTexture : public Texture {
  Q_OBJECT
  Q_PROPERTY(Texture* brightTexture READ brightTexture WRITE setBrightTexture)
  Q_PROPERTY(Texture* darkTexture READ darkTexture WRITE setDarkTexture)
  Q_PROPERTY(QString mapping READ mapping WRITE setMapping)
  Q_PROPERTY(double uScale READ uScale WRITE setUScale)
  Q_PROPERTY(double vScale READ vScale WRITE setVScale)
  
public:
  /**
    * Default constructor. Creates a checker board texture with no brigth and
    * dark textures.
    */
  explicit CheckerBoardTexture(Element* parent = nullptr);

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
    * <td>@image html checker_board_bright_color_red.png "red"</td>
    * <td>@image html checker_board_bright_color_orange.png "orange"</td>
    * <td>@image html checker_board_bright_color_yellow.png "yellow"</td>
    * <td>@image html checker_board_bright_color_green.png "green"</td>
    * <td>@image html checker_board_bright_color_blue.png "blue"</td>
    * <td>@image html checker_board_bright_color_indigo.png "indigo"</td>
    * <td>@image html checker_board_bright_color_violet.png "violet"</td>
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
    * <td>@image html checker_board_dark_color_red.png "red"</td>
    * <td>@image html checker_board_dark_color_orange.png "orange"</td>
    * <td>@image html checker_board_dark_color_yellow.png "yellow"</td>
    * <td>@image html checker_board_dark_color_green.png "green"</td>
    * <td>@image html checker_board_dark_color_blue.png "blue"</td>
    * <td>@image html checker_board_dark_color_indigo.png "indigo"</td>
    * <td>@image html checker_board_dark_color_violet.png "violet"</td>
    * </tr></table>
    */
  inline void setDarkTexture(Texture* texture) {
    if (texture == this) {
      m_darkTexture = nullptr;
    } else {
      m_darkTexture = texture;
    }
  }

  /**
    * @returns the coordinate mapping used before the checker test.
    * Supported values are "planar" and "uv".
    */
  inline const QString& mapping() const {
    return m_mapping;
  }

  inline void setMapping(const QString& mapping) {
    m_mapping = mapping == "uv" ? "uv" : "planar";
  }

  inline double uScale() const {
    return m_uScale;
  }

  inline void setUScale(double scale) {
    m_uScale = scale;
  }

  inline double vScale() const {
    return m_vScale;
  }

  inline void setVScale(double scale) {
    m_vScale = scale;
  }
  
  virtual std::shared_ptr<render::Texturec> toRaytracerTexture() const;

private:
  Colord m_color;
  Texture* m_brightTexture;
  Texture* m_darkTexture;
  QString m_mapping;
  double m_uScale;
  double m_vScale;
};
