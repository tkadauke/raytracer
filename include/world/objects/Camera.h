#pragma once

#include "world/objects/Element.h"
#include "core/math/Vector.h"

namespace raytracer {
  class Camera;
}

class Camera : public Element {
  Q_OBJECT;
  Q_PROPERTY(Vector3d position READ position WRITE setPosition);
  Q_PROPERTY(Vector3d target READ target WRITE setTarget);
  
public:
  Camera(Element* parent = nullptr);
  
  inline const Vector3d& position() const {
    return m_position;
  }
  
  inline void setPosition(const Vector3d& position) {
    m_position = position;
  }
  
  inline const Vector3d& target() const {
    return m_target;
  }
  
  inline void setTarget(const Vector3d& target) {
    m_target = target;
  }
  
  virtual std::shared_ptr<raytracer::Camera> toRaytracer() const = 0;
  
private:
  Vector3d m_position;
  Vector3d m_target;
};
