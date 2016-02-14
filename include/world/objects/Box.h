#pragma once

#include "world/objects/Surface.h"

class Box : public Surface {
  Q_OBJECT
  
public:
  Box(Element* parent);

  virtual raytracer::Primitive* toRaytracerPrimitive() const;
};
