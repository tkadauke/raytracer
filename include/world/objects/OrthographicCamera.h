#pragma once

#include "world/objects/Camera.h"

class OrthographicCamera : public Camera {
  Q_OBJECT;
  Q_PROPERTY(double zoom READ zoom WRITE setZoom);
  
public:
  OrthographicCamera(Element* parent = nullptr);

  inline double zoom() const {
    return m_zoom;
  }
  
  inline void setZoom(double zoom) {
    if (zoom <= 0) {
      m_zoom = 1;
    } else {
      m_zoom = zoom;
    }
  }

  virtual std::shared_ptr<raytracer::Camera> toRaytracer() const;

private:
  double m_zoom;
};
