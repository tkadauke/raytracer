#pragma once

#include <vector>

#include "raytracer/primitives/Composite.h"

namespace raytracer {
  class Grid : public Composite {
  public:
    inline Grid()
      : m_numX(0),
        m_numY(0),
        m_numZ(0)
    {
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const;
    virtual bool intersects(const Rayd& ray, State& state) const;

    void setup();

  private:
    std::vector<std::shared_ptr<Primitive>> m_cells;
    int m_numX, m_numY, m_numZ;

    BoundingBoxd m_boundingBox;
  };
}
