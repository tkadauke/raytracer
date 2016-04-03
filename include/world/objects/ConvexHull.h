#pragma once

#include "world/objects/CSGSurface.h"

/**
  * Represents a convex hull operation.
  * 
  * @image html convex_hull.png "Convex hull of two spheres"
  */
class ConvexHull : public CSGSurface {
  Q_OBJECT;
  
public:
  /**
    * Default constructor. Creates an empty convex hull.
    */
  explicit ConvexHull(Element* parent = nullptr);
  
  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const;
};
