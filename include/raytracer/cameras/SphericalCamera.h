#ifndef RAYTRACER_SPHERICAL_CAMERA_H
#define RAYTRACER_SPHERICAL_CAMERA_H

#include "raytracer/cameras/Camera.h"

namespace raytracer {
  class ViewPlane;

  class SphericalCamera : public Camera {
  public:
    SphericalCamera()
      : m_horizontalFieldOfView(180), m_verticalFieldOfView(120) {}
    SphericalCamera(float horizontalFieldOfView, float verticalFieldOfView)
      : m_horizontalFieldOfView(horizontalFieldOfView), m_verticalFieldOfView(verticalFieldOfView) {}
    SphericalCamera(const Vector3d& position, const Vector3d& target)
      : Camera(position, target), m_horizontalFieldOfView(180), m_verticalFieldOfView(120) {}

    using Camera::render;
    virtual void render(Raytracer* raytracer, Buffer<unsigned int>& buffer, const Rect& rect);
    virtual Ray rayForPixel(int x, int y);

    inline void setHorizontalFieldOfView(float fov) { m_horizontalFieldOfView = fov; }
    inline float horizontalFieldOfView() const { return m_horizontalFieldOfView; }

    inline void setVerticalFieldOfView(float fov) { m_verticalFieldOfView = fov; }
    inline float verticalFieldOfView() const { return m_verticalFieldOfView; }

    inline void setFieldOfView(float horizontal, float vertical) {
      setHorizontalFieldOfView(horizontal);
      setVerticalFieldOfView(vertical);
    }

  private:
    Vector3d direction(const ViewPlane& plane, int x, int y);

    float m_horizontalFieldOfView;
    float m_verticalFieldOfView;
  };
}

#endif