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
  Q_PROPERTY(double bevelRadius READ bevelRadius WRITE setBevelRadius);
  
public:
  /**
    * Default constructor. Creates a box with size (1, 1, 1) around the origin.
    */
  explicit Box(Element* parent = nullptr);

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
    * <td>@image html box_size_1.png "1"</td>
    * <td>@image html box_size_2.png "2"</td>
    * <td>@image html box_size_3.png "3"</td>
    * <td>@image html box_size_4.png "4"</td>
    * <td>@image html box_size_5.png "5"</td>
    * </tr></table>
    */
  inline void setSize(const Vector3d& size) {
    m_size = Vector3d(
      std::max(std::abs(size.x()), 0.000001),
      std::max(std::abs(size.y()), 0.000001),
      std::max(std::abs(size.z()), 0.000001)
    );
    // recalculate bevel radius, in case the size shrunk so far that the
    // previous radius is too big
    setBevelRadius(bevelRadius());
  }
  
  /**
    * @returns the bevel radius.
    */
  inline double bevelRadius() const {
    return m_bevelRadius;
  }
  
  /**
    * Sets the bevel radius of the box. If the radius is 0, the box will be a
    * simple box primitive with perfectly sharp edges. If the radius is greater
    * than 0, the edges and corners will be rounded.
    * 
    * <table><tr>
    * <td>@image html box_bevel_radius_1.png "1"</td>
    * <td>@image html box_bevel_radius_2.png "2"</td>
    * <td>@image html box_bevel_radius_3.png "3"</td>
    * <td>@image html box_bevel_radius_4.png "4"</td>
    * <td>@image html box_bevel_radius_5.png "5"</td>
    * </tr></table>
    */
  inline void setBevelRadius(double radius) {
    m_bevelRadius = std::min(size().min(), radius);
  }

  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const;

private:
  Vector3d m_size;
  double m_bevelRadius;
};
