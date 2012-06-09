#ifndef GRID_H
#define GRID_H

#include <vector>

#include "surfaces/Composite.h"

class Grid : public Composite {
public:
  inline Grid() {}
  // ~Grid();
  
  virtual Surface* intersect(const Ray& ray, HitPointInterval& hitPoints);
  virtual bool intersects(const Ray& ray);
  
  void setup();

private:
  std::vector<Surface*> m_cells;
  int m_numX, m_numY, m_numZ;
  
  BoundingBox m_boundingBox;
};

#endif
