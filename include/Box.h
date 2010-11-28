#ifndef BOX_H
#define BOX_H

#include "Surface.h"
#include "Vector.h"

class Box : public Surface {
public:
  Box(const Vector3d& center, const Vector3d& edge)
    : m_center(center), m_edge(edge)
  {
  }
  
  virtual Surface* intersect(const Ray& ray, HitPointInterval& hitPoints);

private:
  Vector3d m_center, m_edge;
};

#endif
