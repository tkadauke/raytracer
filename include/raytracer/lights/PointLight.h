#pragma once

#include "raytracer/lights/Light.h"
#include "core/Color.h"

namespace raytracer {
  class PointLight : public Light {
  public:
    inline PointLight(const Vector3d& position, const Colord& color)
      : Light(),
        m_position(position),
        m_color(color)
    {
    }

    inline const Vector3d& position() const {
      return m_position;
    }
    
    inline const Colord& color() const {
      return m_color;
    }
    
    virtual Vector3d direction(const Vector3d& point) const;
    virtual Colord radiance() const;
    
  private:
    Vector3d m_position;
    Colord m_color;
  };
}
