#pragma once

#include "world/objects/Surface.h"

/**
  * Represents a box primitive.
  * 
  * @image html box.png "Box with origin (0, 0, 0) and size (1, 1, 1)"
  */
class Box : public Surface {
  Q_OBJECT;
  Q_PROPERTY(Vector3d size READ size WRITE setSize);
  
public:
  /**
    * Default constructor. Creates a box with size (1, 1, 1) around the origin.
    */
  Box(Element* parent = nullptr);

  /**
    * @returns the half size of the box, which is the difference between the
    *   center of the box and a corner. All components of the returned vector
    *   are strictly positive.
    */
  inline const Vector3d& size() const {
    return m_size;
  }
  
  /**
    * Sets the size of the box. All three components will be converted to their
    * absolute value. A component of 0 will be replaced with \f$\epsilon\f$.
    * 
    * <table><tr>
    * <td>@image html box_size_1.png</td>
    * <td>@image html box_size_2.png</td>
    * <td>@image html box_size_3.png</td>
    * <td>@image html box_size_4.png</td>
    * <td>@image html box_size_5.png</td>
    * </tr></table>
    */
  inline void setSize(const Vector3d& size) {
    m_size = Vector3d(
      std::max(std::abs(size.x()), std::numeric_limits<double>::epsilon()),
      std::max(std::abs(size.y()), std::numeric_limits<double>::epsilon()),
      std::max(std::abs(size.z()), std::numeric_limits<double>::epsilon())
    );
  }

  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const;

private:
  Vector3d m_size;
};
