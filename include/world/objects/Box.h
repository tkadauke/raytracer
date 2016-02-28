#pragma once

#include "world/objects/Surface.h"

class Box : public Surface {
  Q_OBJECT;
  
public:
  Box(Element* parent = nullptr);

  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const;
};
