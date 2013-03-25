#ifndef ORTHOGRAPHIC_CAMERA_H
#define ORTHOGRAPHIC_CAMERA_H

#include "raytracer/cameras/Camera.h"

class OrthographicCamera : public Camera {
public:
  OrthographicCamera() {}
  OrthographicCamera(const Vector3d& position, const Vector3d& target)
    : Camera(position, target) {}

  using Camera::render;
  virtual void render(Raytracer* raytracer, Buffer<unsigned int>& buffer, const Rect& rect);
  virtual Ray rayForPixel(int x, int y);
};

#endif
