#pragma once

#include "world/objects/Transformable.h"
#include "core/Color.h"

namespace raytracer {
  class Light;
}

/**
  * Abstract base class for lights. Lights have a visible switch, a color and an
  * intensity. Lights are generally Transformable, but the individual subclasses
  * may lose some of the features that can be expressed with linear
  * transformations, like e.g. the DirectionalLight class will ignore the
  * position part of the transformation.
  */
class Light : public Transformable {
  Q_OBJECT;
  Q_PROPERTY(bool visible READ visible WRITE setVisible);
  Q_PROPERTY(Colord color READ color WRITE setColor);
  Q_PROPERTY(double intensity READ intensity WRITE setIntensity);

public:
  /**
    * Creates a new visible light with white color and an intensity of 0.5.
    */
  explicit Light(Element* parent = nullptr);

  /**
    * @returns true if the light is visible, false otherwise.
    */
  inline bool visible() const {
    return m_visible;
  }

  /**
    * Sets the light's visibility status to visible.
    */
  inline void setVisible(bool visible) {
    m_visible = visible;
  }

  /**
    * Sets the light's visibility to true.
    */
  inline void show() {
    setVisible(true);
  }

  /**
    * Sets the light's visibility to false.
    */
  inline void hide() {
    setVisible(false);
  }

  /**
    * @returns the light's color.
    */
  inline Colord color() const {
    return m_color;
  }

  /**
    * Sets the light's color.
    *
    * <table><tr>
    * <td>@image html light_rainbow_red.png</td>
    * <td>@image html light_rainbow_orange.png</td>
    * <td>@image html light_rainbow_yellow.png</td>
    * <td>@image html light_rainbow_green.png</td>
    * <td>@image html light_rainbow_blue.png</td>
    * <td>@image html light_rainbow_indigo.png</td>
    * <td>@image html light_rainbow_violet.png</td>
    * </tr></table>
    */
  inline void setColor(const Colord& color) {
    m_color = color;
  }

  /**
    * @returns the light's intensity.
    *
    * <table><tr>
    * <td>@image html light_intensity_0.0.png "intensity=0"</td>
    * <td>@image html light_intensity_0.25.png "intensity=0.25"</td>
    * <td>@image html light_intensity_0.5.png "intensity=0.5"</td>
    * <td>@image html light_intensity_0.75.png "intensity=0.75"</td>
    * <td>@image html light_intensity_1.0.png "intensity=1"</td>
    * </tr></table>
    */
  inline double intensity() const {
    return m_intensity;
  }

  /**
    * Sets the light's intensity.
    */
  inline void setIntensity(double intensity) {
    m_intensity = intensity;
  }

  virtual std::shared_ptr<raytracer::Light> toRaytracer() const = 0;

private:
  bool m_visible;
  Colord m_color;
  double m_intensity;
};
