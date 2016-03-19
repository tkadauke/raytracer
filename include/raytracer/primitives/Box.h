#pragma once

#include "raytracer/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace raytracer {
  class Box : public Primitive {
  public:
    inline Box(const Vector3d& center, const Vector3d& edge)
      : m_center(center),
        m_edge(edge)
    {
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const;
  
  protected:
    virtual BoundingBoxd calculateBoundingBox() const;

  private:
    Vector3d m_center, m_edge;
  };
}
