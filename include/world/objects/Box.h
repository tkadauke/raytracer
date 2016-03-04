#pragma once

#include "world/objects/Surface.h"

class Box : public Surface {
  Q_OBJECT;
  Q_PROPERTY(Vector3d size READ size WRITE setSize);
  
public:
  Box(Element* parent = nullptr);

  inline const Vector3d& size() const {
    return m_size;
  }
  
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
