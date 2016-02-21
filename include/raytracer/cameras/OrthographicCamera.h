#pragma once

#include "raytracer/cameras/Camera.h"

namespace raytracer {
  class OrthographicCamera : public Camera {
  public:
    OrthographicCamera() {}
    OrthographicCamera(const Vector3d& position, const Vector3d& target)
      : Camera(position, target) {}

    using Camera::render;
    virtual void render(std::shared_ptr<Raytracer> raytracer, Buffer<unsigned int>& buffer, const Rect& rect);
    virtual Ray rayForPixel(int x, int y);
  };
}
