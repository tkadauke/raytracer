#ifndef LIGHT_H
#define LIGHT_H

#include "core/math/Vector.h"
#include "core/Color.h"

namespace raytracer {
  class Light {
  public:
    Light(const Vector3d& position, const Colord& color)
      : m_position(position), m_color(color)
    {
    }

    virtual ~Light() {}

    const Vector3d& position() const { return m_position; }
    const Colord& color() const { return m_color; }

  private:
    Vector3d m_position;
    Colord m_color;
  };
}

#endif
