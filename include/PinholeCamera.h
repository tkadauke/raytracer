#ifndef PINHOLE_CAMERA_H
#define PINHOLE_CAMERA_H

#include "Camera.h"

class PinholeCamera : public Camera {
public:
  PinholeCamera() {}
  PinholeCamera(const Vector3d& position, const Vector3d& target)
    : Camera(position, target) {}

  virtual void render(Raytracer* raytracer, Buffer& buffer);
};

#endif
