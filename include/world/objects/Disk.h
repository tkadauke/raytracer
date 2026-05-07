#pragma once
#include <algorithm>
#include <memory>

#include "world/objects/Surface.h"

/**
  * Represents a disk primitive — a flat circular surface in the
  * local XZ plane (normal points along +Y) with the configured
  * `radius`. Position and orientation are inherited from
  * `Transformable`.
  *
  * @image html disk_wireframe.png "Disk rendered through WireframeEngine"
  */
class Disk : public Surface {
  Q_OBJECT;
  Q_PROPERTY(double radius READ radius WRITE setRadius);

public:
  explicit Disk(Element* parent = nullptr);

  inline double radius() const { return m_radius; }

  /// Sets the disk radius. Coerced to its absolute value with an
  /// epsilon floor.
  inline void setRadius(double radius) {
    m_radius = std::max(std::abs(radius), std::numeric_limits<double>::epsilon());
  }

  virtual std::shared_ptr<render::Primitive> toRaytracerPrimitive() const;

private:
  double m_radius;
};
