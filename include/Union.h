#ifndef UNION_H
#define UNION_H

#include "Composite.h"

class Union : public Composite {
public:
  virtual Surface* intersect(const Ray& ray, HitPointInterval& hitPoints);
  virtual bool intersects(const Ray& ray);
};

#endif
