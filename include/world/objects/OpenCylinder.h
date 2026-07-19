#pragma once
#include <memory>

#include "world/objects/Surface.h"

/**
  * Represents an open cylinder primitive — the side surface only,
  * no end caps, axis along local +Y. For a closed cylinder (with
  * disk caps), use `Cylinder`.
  *
  * <table><tr>
  * <td>@image html open_cylinder__raytracer.png "Raytracer"</td>
  * <td>@image html open_cylinder__raster.png "Rasterizer"</td>
  * <td>@image html open_cylinder__wireframe.png "Wireframe"</td>
  * </tr></table>
  */
class OpenCylinder : public Surface {
  Q_OBJECT
  Q_PROPERTY(double radius READ radius WRITE setRadius)
  Q_PROPERTY(double height READ height WRITE setHeight)

public:
  explicit OpenCylinder(Element* parent = nullptr);

  inline double radius() const {
    return m_radius;
  }

  /// Sets the cylinder radius. Coerced to its absolute value with
  /// an epsilon floor.
  inline void setRadius(double radius) {
    m_radius = positiveExtent(radius);
  }

  inline double height() const {
    return m_height;
  }

  /// Sets the cylinder height. Coerced to its absolute value with
  /// an epsilon floor.
  inline void setHeight(double height) {
    m_height = positiveExtent(height);
  }

  std::shared_ptr<render::Primitive> toRaytracerPrimitive() const override;

private:
  double m_radius;
  double m_height;
};
