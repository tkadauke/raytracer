#pragma once

#include "world/objects/Surface.h"

/**
  * Base class for all constructive solid geometry classes.
  *
  * @image html csg_surface.png "Complex CSG object"
  */
class CSGSurface : public Surface {
  Q_OBJECT
  Q_PROPERTY(bool active READ active WRITE setActive)

public:
  /**
    * Default constructor.
    */
  explicit CSGSurface(Element* parent = nullptr);

  /**
    * @returns true if the CSG operation is active, false otherwise.
    */
  inline bool active() const {
    return m_active;
  }

  /**
    * Activates or deactivates the CSG operation. If the CSG operation is
    * inactive, this class effectively behaves like a simple composite, which
    * means all children's geometries are unchanged. If the CSG operation is
    * active, the children's geometries are changed according to the operation.
    *
    * @image html csg_surface_inactive.png "Inactive CSG object"
    */
  inline void setActive(bool active) {
    m_active = active;
  }

protected:
  /**
    * Shared body for every CSG subclass's `toRaytracerPrimitive()`: builds a
    * named @p T when active with children, `nullptr` when active with no
    * children, and a plain `render::Composite` when inactive.
    */
  template<class T>
  inline std::shared_ptr<render::Primitive> csgToRaytracerPrimitive() const {
    if (active()) {
      if (children().size() > 0) {
        return make_named<T>();
      } else {
        return nullptr;
      }
    } else {
      return csgInactivePrimitive();
    }
  }

private:
  /// Out-of-line so callers of the template above don't need
  /// `render::Composite` to be a complete type (e.g. moc-generated code
  /// that only sees this header, not `render/primitives/Composite.h`).
  std::shared_ptr<render::Primitive> csgInactivePrimitive() const;


  bool m_active;
};
