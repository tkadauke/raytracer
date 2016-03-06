#pragma once

#include "world/objects/CSGSurface.h"

class Intersection : public CSGSurface {
  Q_OBJECT;
  
public:
  Intersection(Element* parent = nullptr);
  
  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const;
};
