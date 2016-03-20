#pragma once

#include "world/objects/CSGSurface.h"

/**
  * Represents a CSG difference operation. The first child is the object from
  * which all other children's geometries are subtracted.
  * 
  * @image html difference.png "Difference between a red box and a blue sphere"
  */
class Difference : public CSGSurface {
  Q_OBJECT;
  
public:
  /**
    * Default constructor. Creates an empty difference.
    */
  Difference(Element* parent = nullptr);
  
  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const;
};
