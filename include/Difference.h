#ifndef DIFFERENCE_H
#define DIFFERENCE_H

#include "Composite.h"

class Difference : public Composite {
public:
  virtual Surface* intersect(const Ray& ray, HitPointInterval& hitPoints);
};

#endif
