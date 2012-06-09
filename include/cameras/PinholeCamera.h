#ifndef PINHOLE_CAMERA_H
#define PINHOLE_CAMERA_H

#include "cameras/Camera.h"

class PinholeCamera : public Camera {
public:
  PinholeCamera() : Camera(), m_distance(5) {}
  PinholeCamera(const Vector3d& position, const Vector3d& target)
    : Camera(position, target), m_distance(5) {}

  using Camera::render;
  virtual void render(Raytracer* raytracer, Buffer<unsigned int>& buffer, const Rect& rect);
  
  inline void setDistance(double distance) { m_distance = distance; }
  inline double distance() const { return m_distance; }

private:
  double m_distance;
};

#endif
