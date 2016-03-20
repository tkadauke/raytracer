#pragma once

#include "world/objects/Surface.h"

/**
  * Base class for all constructive solid geometry classes.
  * 
  * @image html csg_surface.png "Complex CSG object"
  */
class CSGSurface : public Surface {
  Q_OBJECT;
  Q_PROPERTY(bool active READ active WRITE setActive);
  
public:
  /**
    * Default constructor.
    */
  CSGSurface(Element* parent = nullptr);
  
  /**
    * @returns true if the CSG operation is active, false otherwise.
    */
  bool active() const { return m_active; }
  
  /**
    * Activates or deactivates the CSG operation. If the CSG operation is
    * inactive, this class effectively behaves like a simple composite, which
    * means all children's geometries are unchanged. If the CSG operation is
    * active, the children's geometries are changed according to the operation.
    * 
    * @image html csg_surface_inactive.png "Inactive CSG object"
    */
  void setActive(bool active) { m_active = active; }
  
private:
  bool m_active;
};
