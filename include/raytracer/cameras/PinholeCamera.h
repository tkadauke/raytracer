#ifndef RAYTRACER_PINHOLE_CAMERA_H
#define RAYTRACER_PINHOLE_CAMERA_H

#include "raytracer/cameras/Camera.h"

namespace raytracer {
  class PinholeCamera : public Camera {
  public:
    PinholeCamera() : Camera(), m_distance(5), m_zoom(1) {}
    PinholeCamera(const Vector3d& position, const Vector3d& target)
      : Camera(position, target), m_distance(5), m_zoom(1) {}

    using Camera::render;
    virtual void render(Raytracer* raytracer, Buffer<unsigned int>& buffer, const Rect& rect);
    virtual Ray rayForPixel(int x, int y);

    inline void setDistance(double distance) { m_distance = distance; }
    inline double distance() const { return m_distance; }

    inline void setZoom(double zoom) { m_zoom = zoom; }
    inline double zoom() const { return m_zoom; }

  private:
    double m_distance;
    double m_zoom;
  };
}

#endif
