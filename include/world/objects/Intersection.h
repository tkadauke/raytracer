#pragma once

#include "world/objects/CSGSurface.h"

/**
  * Represents a CSG intersection operation. The resulting geometry is the
  * overlap between all the childrens' geometries.
  * 
  * @image html intersection.png "Intersection between a red box and a blue sphere"
  */
class Intersection : public CSGSurface {
  Q_OBJECT;
  
public:
  /**
    * Default constructor. Creates an empty intersection.
    */
  explicit Intersection(Element* parent = nullptr);
  
  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const;
};
