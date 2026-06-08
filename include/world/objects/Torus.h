#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

#include "world/objects/Surface.h"

/**
  * Represents a torus primitive (donut shape) — a circle of radius
  * `tubeRadius` swept around an axis at distance `sweptRadius` from
  * the centre. The hole points along the local +y axis; rotate via
  * the inherited `rotation` property to lay it flat.
  *
  * <table><tr>
  * <td>@image html torus__raytracer.png "Raytracer"</td>
  * <td>@image html torus__raster.png "Rasterizer"</td>
  * <td>@image html torus__wireframe.png "Wireframe"</td>
  * </tr></table>
  */
class Torus : public Surface {
  Q_OBJECT
  Q_PROPERTY(double sweptRadius READ sweptRadius WRITE setSweptRadius)
  Q_PROPERTY(double tubeRadius READ tubeRadius WRITE setTubeRadius)

public:
  /**
    * Default constructor. Creates a torus with sweptRadius 2 and
    * tubeRadius 1 around the origin.
    */
  explicit Torus(Element* parent = nullptr);

  inline double sweptRadius() const {
    return m_sweptRadius;
  }

  /**
    * Sets the swept radius (distance from torus centre to tube
    * centre). Coerced to its absolute value with an epsilon floor.
    */
  inline void setSweptRadius(double radius) {
    m_sweptRadius = std::max(std::abs(radius), std::numeric_limits<double>::epsilon());
  }

  inline double tubeRadius() const {
    return m_tubeRadius;
  }

  /**
    * Sets the tube radius (radius of the cross-section circle).
    * Coerced to its absolute value with an epsilon floor.
    */
  inline void setTubeRadius(double radius) {
    m_tubeRadius = std::max(std::abs(radius), std::numeric_limits<double>::epsilon());
  }

  std::shared_ptr<render::Primitive> toRaytracerPrimitive() const override;

private:
  double m_sweptRadius;
  double m_tubeRadius;
};
