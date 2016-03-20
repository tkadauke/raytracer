#pragma once

#include "world/objects/CSGSurface.h"

/**
  * Represents a CSG union operation. The resulting geometry is the merged
  * geometry of all the childrens' geometries.
  * 
  * @image html union.png "Union between a red box and a blue sphere"
  */
class Union : public CSGSurface {
  Q_OBJECT;
  
public:
  /**
    * Default constructor. Creates an empty union.
    */
  explicit Union(Element* parent = nullptr);
  
  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const;
};
