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
  
public:
  /**
    * Default constructor. Creates a cylinder with radius 1 around the origin.
    */
  Cylinder(Element* parent = nullptr);
  
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
    * <td>@image html cylinder_radius_1.png</td>
    * <td>@image html cylinder_radius_2.png</td>
    * <td>@image html cylinder_radius_3.png</td>
    * <td>@image html cylinder_radius_4.png</td>
    * <td>@image html cylinder_radius_5.png</td>
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
    * <td>@image html cylinder_height_1.png</td>
    * <td>@image html cylinder_height_2.png</td>
    * <td>@image html cylinder_height_3.png</td>
    * <td>@image html cylinder_height_4.png</td>
    * <td>@image html cylinder_height_5.png</td>
    * </tr></table>
    */
  inline void setHeight(double height) {
    m_height = std::max(std::abs(height), std::numeric_limits<double>::epsilon());
  }

  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const;
  
private:
  double m_radius;
  double m_height;
};
