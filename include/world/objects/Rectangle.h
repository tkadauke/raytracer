#pragma once
#include <memory>

#include "world/objects/Surface.h"

/**
  * Represents a rectangle primitive — a flat quad defined by a
  * corner (taken from `Transformable::position`) and two edge
  * vectors `leg1` and `leg2`. The rectangle's normal is the
  * cross product of the two legs (so swapping them flips the
  * normal direction).
  *
  * <table><tr>
  * <td>@image html rectangle__raytracer.png "Raytracer"</td>
  * <td>@image html rectangle__raster.png "Rasterizer"</td>
  * <td>@image html rectangle__wireframe.png "Wireframe"</td>
  * </tr></table>
  */
class Rectangle : public Surface {
  Q_OBJECT
  Q_PROPERTY(Vector3d leg1 READ leg1 WRITE setLeg1)
  Q_PROPERTY(Vector3d leg2 READ leg2 WRITE setLeg2)

public:
  explicit Rectangle(Element* parent = nullptr);

  inline const Vector3d& leg1() const {
    return m_leg1;
  }
  inline const Vector3d& leg2() const {
    return m_leg2;
  }

  inline void setLeg1(const Vector3d& v) {
    m_leg1 = v;
  }
  inline void setLeg2(const Vector3d& v) {
    m_leg2 = v;
  }

  virtual std::shared_ptr<render::Primitive> toRaytracerPrimitive() const;

private:
  Vector3d m_leg1;
  Vector3d m_leg2;
};
