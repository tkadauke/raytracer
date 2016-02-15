#pragma once

#include "world/objects/Surface.h"

class Sphere : public Surface {
  Q_OBJECT
  
public:
  Sphere(Element* parent = nullptr);

  virtual raytracer::Primitive* toRaytracerPrimitive() const;
};
