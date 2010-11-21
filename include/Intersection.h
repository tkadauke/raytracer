#ifndef INTERSECTION_H
#define INTERSECTION_H

#include "Composite.h"

class Intersection : public Composite {
public:
  virtual Surface* intersect(const Ray& ray, HitPointInterval& hitPoints);
};

#endif
