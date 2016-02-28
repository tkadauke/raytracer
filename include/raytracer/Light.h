#pragma once

#include "core/math/Vector.h"
#include "core/Color.h"

namespace raytracer {
  class Light {
  public:
    inline Light(const Vector3d& position, const Colord& color)
      : m_position(position),
        m_color(color)
    {
    }

    inline virtual ~Light() {}

    inline const Vector3d& position() const {
      return m_position;
    }
    
    inline const Colord& color() const {
      return m_color;
    }
    
  private:
    Vector3d m_position;
    Colord m_color;
  };
}
