#ifndef SURFACE_H
#define SURFACE_H

#include "Material.h"

class Ray;
class HitPointInterval;

class Surface {
public:
  Surface() {}
  virtual ~Surface() {}
  
  virtual Surface* intersect(const Ray& ray, HitPointInterval& hitPoints) = 0;
  
  inline Material& material() { return m_material; }

private:
  Material m_material;
};

#endif
