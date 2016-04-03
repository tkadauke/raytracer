#pragma once

#include "world/objects/CSGSurface.h"

/**
  * Represents a Minkowski sum operation.
  * 
  * @image html minkowski_sum.png "Minkowski sum of a box and a cylinder"
  */
class MinkowskiSum : public CSGSurface {
  Q_OBJECT;
  
public:
  /**
    * Default constructor. Creates an empty Minkowski sum.
    */
  explicit MinkowskiSum(Element* parent = nullptr);
  
  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const;
};
