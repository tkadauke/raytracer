#pragma once
#include <algorithm>
#include <memory>

#include "world/objects/Surface.h"

/**
  * Represents an open cylinder primitive — the side surface only,
  * no end caps, axis along local +Y. For a closed cylinder (with
  * disk caps), use `Cylinder`.
  *
  * @image html open_cylinder_wireframe.png "OpenCylinder rendered through WireframeEngine"
  */
class OpenCylinder : public Surface {
  Q_OBJECT;
  Q_PROPERTY(double radius READ radius WRITE setRadius);
  Q_PROPERTY(double height READ height WRITE setHeight);

public:
  explicit OpenCylinder(Element* parent = nullptr);

  inline double radius() const { return m_radius; }

  /// Sets the cylinder radius. Coerced to its absolute value with
  /// an epsilon floor.
  inline void setRadius(double radius) {
    m_radius = std::max(std::abs(radius), std::numeric_limits<double>::epsilon());
  }

  inline double height() const { return m_height; }

  /// Sets the cylinder height. Coerced to its absolute value with
  /// an epsilon floor.
  inline void setHeight(double height) {
    m_height = std::max(std::abs(height), std::numeric_limits<double>::epsilon());
  }

  virtual std::shared_ptr<raytracer::Primitive> toRaytracerPrimitive() const;

private:
  double m_radius;
  double m_height;
};
