#ifndef ORTHOGRAPHIC_CAMERA_H
#define ORTHOGRAPHIC_CAMERA_H

#include "Camera.h"

class OrthographicCamera : public Camera {
public:
  OrthographicCamera() {}
  OrthographicCamera(const Vector3d& position, const Vector3d& target)
    : Camera(position, target) {}

  virtual void render(Raytracer* raytracer, Buffer& buffer);
};

#endif
