#ifndef DIFFERENCE_H
#define DIFFERENCE_H

#include "surfaces/Composite.h"

class Difference : public Composite {
public:
  virtual Surface* intersect(const Ray& ray, HitPointInterval& hitPoints);
};

#endif
