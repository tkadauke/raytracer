#pragma once

#include "world/objects/Surface.h"

/**
  * Represents a cylinder.
  * 
  * @image html cylinder.png "Cylinder with origin (0, 0, 0), radius 1 and height 2"
  */
class Cylinder : public Surface {
  Q_OBJECT;
  Q_PROPERTY(double radius READ radius WRITE setRadius);
  Q_PROPERTY(double height READ height WRITE setHeight);
  Q_PROPERTY(double bevelRadius READ bevelRadius WRITE setBevelRadius);
  
public:
  /**
    * Default constructor. Creates a cylinder with radius 1 around the origin.
    */
  explicit Cylinder(Element* parent = nullptr);
  
  /**
    * @returns the radius of the cylinder.
    */
  inline double radius() const {
    return m_radius;
  }
  
  /**
    * Sets the radius of the cylinder. The radius will be converted to its
    * absolute value. A radius of 0 will be replaced with \f$\epsilon\f$.
    * 
    * <table><tr>
    * <td>@image html cylinder_radius_1.png "Radius 1"</td>
    * <td>@image html cylinder_radius_2.png "Radius 2"</td>
    * <td>@image html cylinder_radius_3.png "Radius 3"</td>
    * <td>@image html cylinder_radius_4.png "Radius 4"</td>
    * <td>@image html cylinder_radius_5.png "Radius 5"</td>
    * </tr></table>
    */
  inline void setRadius(double radius) {
    m_radius = std::max(std::abs(radius), std::numeric_limits<double>::epsilon());
  }

  /**
    * @returns the height of the cylinder.
    */
  inline double height() const {
    return m_height;
  }

  /**
    * Sets the height of the cylinder. The height will be converted to its
    * absolute value. A height of 0 will be replaced with \f$\epsilon\f$.
    * 
    * <table><tr>
    * <td>@image html cylinder_height_1.png "Height 1"</td>
    * <td>@image html cylinder_height_2.png "Height 2"</td>
    * <td>@image html cylinder_height_3.png "Height 3"</td>
    * <td>@image html cylinder_height_4.png "Height 4"</td>
    * <td>@image html cylinder_height_5.png "Height 5"</td>
    * </tr></table>
    */
  inline void setHeight(double height) {
    m_height = std::max(std::abs(height), std::numeric_limits<double>::epsilon());
  }

  /**
    * @returns the bevel radius.
    */
  inline double bevelRadius() const {
    return m_bevelRadius;
  }
  
  /**
    * Sets the bevel radius of the cylinder. If the radius is 0, the cylinder
    * will be a simple cylinder primitive with perfectly sharp edges. If the
    * radius is greater than 0, the edges will be rounded.
    * 
    * <table><tr>
    * <td>@image html cylinder_bevel_radius_1.png "Bevel Radius 1"</td>
    * <td>@image html cylinder_bevel_radius_2.png "Bevel Radius 2"</td>
    * <td>@image html cylinder_bevel_radius_3.png "Bevel Radius 3"</td>
    * <td>@image html cylinder_bevel_radius_4.png "Bevel Radius 4"</td>
    * <td>@image html cylinder_bevel_radius_5.png "Bevel Radius 5"</td>
    * </tr></table>
    */
  inline void setBevelRadius(double radius) {
    m_bevelRadius = std::min(std::min(m_radius, m_height / 2.0), radius);
  }

  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const;
  
private:
  double m_radius;
  double m_height;
  double m_bevelRadius;
};
