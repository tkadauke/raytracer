#pragma once

#include "world/objects/Surface.h"

/**
  * Represents a ring.
  * 
  * @image html ring.png "Ring with origin (0, 0, 0), outer radius 1, inner radius 0.5 and height 2"
  */
class Ring : public Surface {
  Q_OBJECT;
  Q_PROPERTY(double outerRadius READ outerRadius WRITE setOuterRadius);
  Q_PROPERTY(double innerRadius READ innerRadius WRITE setInnerRadius);
  Q_PROPERTY(double height READ height WRITE setHeight);
  Q_PROPERTY(double bevelRadius READ bevelRadius WRITE setBevelRadius);
  
public:
  /**
    * Default constructor. Creates a ring with radius 1 around the origin.
    */
  explicit Ring(Element* parent = nullptr);
  
  /**
    * @returns the outer radius of the ring.
    */
  inline double outerRadius() const {
    return m_outerRadius;
  }
  
  /**
    * Sets the outer radius of the ring. The radius will be converted to its
    * absolute value. A radius of 0 will be replaced with \f$\epsilon\f$. If the
    * outer radius is smaller than the inner radius, it will be set to the inner
    * radius plus \f$\epsilon\f$.
    * 
    * <table><tr>
    * <td>@image html ring_outer_radius_1.png "Radius 1"</td>
    * <td>@image html ring_outer_radius_2.png "Radius 2"</td>
    * <td>@image html ring_outer_radius_3.png "Radius 3"</td>
    * <td>@image html ring_outer_radius_4.png "Radius 4"</td>
    * <td>@image html ring_outer_radius_5.png "Radius 5"</td>
    * </tr></table>
    */
  inline void setOuterRadius(double radius) {
    m_outerRadius = std::max(
      std::max(std::abs(radius), std::numeric_limits<double>::epsilon()),
      innerRadius() + std::numeric_limits<double>::epsilon()
    );
  }

  /**
    * @returns the inner radius of the ring.
    */
  inline double innerRadius() const {
    return m_innerRadius;
  }

  /**
    * Sets the inner radius of the ring. The radius will be converted to its
    * absolute value. A radius of 0 will be replaced with \f$\epsilon\f$. If the
    * inner radius is larger than the outer radius, it will be set to the outer
    * radius minus \f$\epsilon\f$.
    * 
    * <table><tr>
    * <td>@image html ring_inner_radius_1.png "Radius 1"</td>
    * <td>@image html ring_inner_radius_2.png "Radius 2"</td>
    * <td>@image html ring_inner_radius_3.png "Radius 3"</td>
    * <td>@image html ring_inner_radius_4.png "Radius 4"</td>
    * <td>@image html ring_inner_radius_5.png "Radius 5"</td>
    * </tr></table>
    */
  inline void setInnerRadius(double radius) {
    m_innerRadius = std::min(
      std::max(std::abs(radius), std::numeric_limits<double>::epsilon()),
      outerRadius() - std::numeric_limits<double>::epsilon()
    );
  }

  /**
    * @returns the height of the ring.
    */
  inline double height() const {
    return m_height;
  }

  /**
    * Sets the height of the ring. The height will be converted to its
    * absolute value. A height of 0 will be replaced with \f$\epsilon\f$.
    * 
    * <table><tr>
    * <td>@image html ring_height_1.png "Height 1"</td>
    * <td>@image html ring_height_2.png "Height 2"</td>
    * <td>@image html ring_height_3.png "Height 3"</td>
    * <td>@image html ring_height_4.png "Height 4"</td>
    * <td>@image html ring_height_5.png "Height 5"</td>
    * </tr></table>
    */
  inline void setHeight(double height) {
    m_height = std::max(std::abs(height), std::numeric_limits<double>::epsilon());
  }

  /**
    * @returns the bevel radius.
    */
  inline double bevelRadius() const {
    return std::min(std::min((m_outerRadius - m_innerRadius) / 2.0, m_height / 2.0), m_bevelRadius);
  }
  
  /**
    * Sets the bevel radius of the ring. If the radius is 0, the ring will be a
    * simple ring primitive with perfectly sharp edges. If the radius is greater
    * than 0, the edges will be rounded.
    * 
    * <table><tr>
    * <td>@image html ring_bevel_radius_1.png "Bevel Radius 1"</td>
    * <td>@image html ring_bevel_radius_2.png "Bevel Radius 2"</td>
    * <td>@image html ring_bevel_radius_3.png "Bevel Radius 3"</td>
    * <td>@image html ring_bevel_radius_4.png "Bevel Radius 4"</td>
    * <td>@image html ring_bevel_radius_5.png "Bevel Radius 5"</td>
    * </tr></table>
    */
  inline void setBevelRadius(double radius) {
    m_bevelRadius = radius;
  }

  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const;
  
private:
  std::shared_ptr<raytracer::Primitive> closedCylinder(double radius, double height) const;
  std::shared_ptr<raytracer::Primitive> ring(double outerRadius, double innerRadius, double height) const;
  
  double m_innerRadius;
  double m_outerRadius;
  double m_height;
  double m_bevelRadius;
};
