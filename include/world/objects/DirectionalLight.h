#pragma once

#include "world/objects/Light.h"

/**
  * Represents a directional light. A directional light only has a direction
  * and no position, which means that all the light rays are parallel. Use this
  * light to approximate light sources like the sun.
  */
class DirectionalLight : public Light {
  Q_OBJECT;
  Q_PROPERTY(Vector3d direction READ direction WRITE setDirection);
  
public:
  /**
    * Default constructor. Creates a directional light with direction
    * \f$((-0.5, -1, -0.5))\f$.
    */
  explicit DirectionalLight(Element* parent = nullptr);
  
  /**
    * @returns the light's direction.
    */
  inline const Vector3d& direction() const {
    return m_direction;
  }
  
  /**
    * Sets the direction of the light.
    * 
    * <table><tr>
    * <td>@image html directional_light_direction_1.png</td>
    * <td>@image html directional_light_direction_2.png</td>
    * <td>@image html directional_light_direction_3.png</td>
    * <td>@image html directional_light_direction_4.png</td>
    * <td>@image html directional_light_direction_5.png</td>
    * </tr></table>
    */
  inline void setDirection(const Vector3d& direction) {
    m_direction = direction;
  }

  virtual raytracer::Light* toRaytracer() const;

private:
  Vector3d m_direction;
};
