#ifndef INTERSECTION_H
#define INTERSECTION_H

#include "surfaces/Composite.h"

class Intersection : public Composite {
public:
  virtual Surface* intersect(const Ray& ray, HitPointInterval& hitPoints);
  virtual BoundingBox boundingBox();
};

#endif
