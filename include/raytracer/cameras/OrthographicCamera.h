#pragma once

#include "raytracer/cameras/Camera.h"

namespace raytracer {
  class OrthographicCamera : public Camera {
  public:
    OrthographicCamera() : m_zoom(1) {}
    OrthographicCamera(const Vector3d& position, const Vector3d& target)
      : Camera(position, target), m_zoom(1) {}

    using Camera::render;
    virtual void render(std::shared_ptr<Raytracer> raytracer, Buffer<unsigned int>& buffer, const Rect& rect);
    virtual Ray rayForPixel(double x, double y);

    inline void setZoom(double zoom) { m_zoom = zoom; }
    inline double zoom() const { return m_zoom; }

  private:
    double m_zoom;
  };
}
