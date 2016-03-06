#pragma once

#include "world/objects/CSGSurface.h"

class Union : public CSGSurface {
  Q_OBJECT;
  
public:
  Union(Element* parent = nullptr);
  
  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const;
};
