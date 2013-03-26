#ifndef RAYTRACER_GRID_H
#define RAYTRACER_GRID_H

#include <vector>

#include "raytracer/primitives/Composite.h"

namespace raytracer {
  class Grid : public Composite {
  public:
    inline Grid() {}

    virtual Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints);
    virtual bool intersects(const Ray& ray);

    void setup();

  private:
    std::vector<Primitive*> m_cells;
    int m_numX, m_numY, m_numZ;

    BoundingBox m_boundingBox;
  };
}

#endif
