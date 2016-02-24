#pragma once

#include "raytracer/cameras/Camera.h"

namespace raytracer {
  class OrthographicCamera : public Camera {
  public:
    OrthographicCamera() {}
    OrthographicCamera(const Vector3d& position, const Vector3d& target)
      : Camera(position, target) {}

    virtual Ray rayForPixel(double x, double y);
  };
}
