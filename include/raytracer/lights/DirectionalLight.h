#pragma once

#include "raytracer/lights/Light.h"
#include "core/Color.h"

namespace raytracer {
  class DirectionalLight : public Light {
  public:
    inline explicit DirectionalLight(const Vector3d& direction, const Colord& color)
      : Light(),
        m_direction(direction.normalized()),
        m_color(color)
    {
    }

    inline const Vector3d& direction() const {
      return m_direction;
    }
    
    inline const Colord& color() const {
      return m_color;
    }
    
    virtual Vector3d direction(const Vector3d& point) const;
    virtual Colord radiance() const;
    
  private:
    Vector3d m_direction;
    Colord m_color;
  };
}
