#pragma once

#include "world/objects/CSGSurface.h"

class Difference : public CSGSurface {
  Q_OBJECT;
  
public:
  Difference(Element* parent = nullptr);
  
  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const;
};
