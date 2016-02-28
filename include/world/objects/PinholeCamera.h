#pragma once

#include "world/objects/Camera.h"

class PinholeCamera : public Camera {
  Q_OBJECT;
  Q_PROPERTY(double distance READ distance WRITE setDistance);
  Q_PROPERTY(double zoom READ zoom WRITE setZoom);
  
public:
  PinholeCamera(Element* parent = nullptr);
  
  inline double distance() const {
    return m_distance;
  }
  
  inline void setDistance(double distance) {
    m_distance = distance;
  }
  
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
  double m_distance;
  double m_zoom;
};
