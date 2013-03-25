#ifndef DIFFERENCE_H
#define DIFFERENCE_H

#include "primitives/Composite.h"

class Difference : public Composite {
public:
  virtual Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints);
};

#endif
