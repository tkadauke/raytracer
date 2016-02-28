#pragma once

#include "world/objects/Camera.h"
#include "core/math/Angle.h"

class SphericalCamera : public Camera {
  Q_OBJECT;
  Q_PROPERTY(Angled horizontalFieldOfView READ horizontalFieldOfView WRITE setHorizontalFieldOfView);
  Q_PROPERTY(Angled verticalFieldOfView READ verticalFieldOfView WRITE setVerticalFieldOfView);
  
public:
  SphericalCamera(Element* parent = nullptr);
  
  inline void setHorizontalFieldOfView(Angled fov) {
    m_horizontalFieldOfView = fov;
  }
  
  inline const Angled& horizontalFieldOfView() const {
    return m_horizontalFieldOfView;
  }
  
  inline void setVerticalFieldOfView(Angled fov) {
    m_verticalFieldOfView = fov;
  }
  
  inline const Angled& verticalFieldOfView() const {
    return m_verticalFieldOfView;
  }
  
  virtual std::shared_ptr<raytracer::Camera> toRaytracer() const;
  
private:
  Angled m_horizontalFieldOfView;
  Angled m_verticalFieldOfView;
};
